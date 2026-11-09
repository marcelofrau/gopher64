// UWP-PORT: single log sink for the UWP host; see host_log.h. Redirect technique adapted from the
// user's Ymir UWP port (apps/ymir-sdl3/src/platform/uwp/uwp_platform.cpp). (D-02)
#include "host_log.h"

#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
HANDLE g_file_handle = INVALID_HANDLE_VALUE; // raw handle for the crash logger (no CRT locks)
std::thread* g_pump = nullptr;               // heap: a static joinable thread would terminate on exit
volatile LONG g_crash_lines = 0;

int format_prefix(char* out, size_t size, const char* tag) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    return std::snprintf(out, size, "%04u-%02u-%02u %02u:%02u:%02u.%03u [t%lu] [%s] ", now.wYear, now.wMonth,
                         now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, GetCurrentThreadId(), tag);
}

void write_line(const char* tag, const char* text, size_t length) {
    char prefix[96];
    format_prefix(prefix, sizeof(prefix), tag);
    while (length && (text[length - 1] == '\n' || text[length - 1] == '\r')) length--;
    std::string line = prefix;
    line.append(text, length);
    line += '\n';
    std::lock_guard<std::mutex> lock(g_mutex);
    if (IsDebuggerPresent()) OutputDebugStringA(line.c_str());
    if (g_file) {
        std::fwrite(line.data(), 1, line.size(), g_file);
        std::fflush(g_file);
    }
}

// Module containing `address`, by checking the modules a crash is likely to come from. Uses only
// app-partition APIs (GetModuleHandleW + the PE header) instead of the desktop-only lookups.
const wchar_t* module_for(const void* address, uintptr_t* base_out) {
    static const wchar_t* const names[] = {
        nullptr, L"vulkan_dzn.dll", L"dxil.dll", L"SDL3.dll", L"SDL3_ttf.dll", L"d3d12.dll", L"D3D12Core.dll",
        L"dxgi.dll", L"ucrtbase.dll", L"vcruntime140_app.dll", L"msvcp140_app.dll", L"KernelBase.dll", L"ntdll.dll"};
    for (const wchar_t* name : names) {
        auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(name));
        if (!base) continue;
        auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        const auto addr = reinterpret_cast<uintptr_t>(address);
        if (addr >= base && addr < base + nt->OptionalHeader.SizeOfImage) {
            *base_out = base;
            return name ? name : L"gopher64-uwp.exe";
        }
    }
    *base_out = 0;
    return L"?";
}

// Unhandled-exception filter: runs only for exceptions nothing caught (not under a debugger, which
// takes the second chance itself). AddVectoredExceptionHandler is not in the UWP API partition.
LONG WINAPI crash_logger(EXCEPTION_POINTERS* info) {
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (InterlockedIncrement(&g_crash_lines) > 1) return EXCEPTION_CONTINUE_SEARCH;

    uintptr_t base = 0;
    const void* address = info->ExceptionRecord->ExceptionAddress;
    const wchar_t* module = module_for(address, &base);
    char line[512];
    int n = format_prefix(line, sizeof(line), "crash");
    n += std::snprintf(line + n, sizeof(line) - n,
                       "unhandled exception 0x%08lX at %p (%ls+0x%llx); params %llu: 0x%llx 0x%llx\n", code,
                       address, module, static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address) - base),
                       static_cast<unsigned long long>(info->ExceptionRecord->NumberParameters),
                       static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[0]),
                       static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[1]));
    // Raw WriteFile: the faulting thread may hold the CRT or our mutex.
    if (g_file_handle != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(g_file_handle, line, static_cast<DWORD>(n), &written, nullptr);
    }
    if (IsDebuggerPresent()) OutputDebugStringA(line);
    return EXCEPTION_CONTINUE_SEARCH;
}

void redirect_stdio() {
    // The CRT's _pipe is unavailable to UWP apps; wrap a Win32 pipe in CRT descriptors.
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, FALSE};
    HANDLE read_handle = nullptr, write_handle = nullptr;
    if (!CreatePipe(&read_handle, &write_handle, &attributes, 64 * 1024)) {
        host_log("host", "stdio redirect: CreatePipe failed (" + std::to_string(GetLastError()) + ")");
        return;
    }
    const int read_fd = _open_osfhandle(reinterpret_cast<intptr_t>(read_handle), _O_RDONLY | _O_BINARY);
    const int write_fd = _open_osfhandle(reinterpret_cast<intptr_t>(write_handle), _O_WRONLY | _O_BINARY);
    if (read_fd == -1 || write_fd == -1 || _dup2(write_fd, 1) != 0 || _dup2(write_fd, 2) != 0) {
        host_log("host", "stdio redirect: descriptor setup failed; stdout/stderr will be lost");
        if (read_fd != -1) _close(read_fd); else CloseHandle(read_handle);
        if (write_fd != -1) _close(write_fd); else CloseHandle(write_handle);
        return;
    }
    _close(write_fd);
    // Rust's std writes to GetStdHandle(), not CRT descriptors, so point the Win32 handles there too.
    SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(1)));
    SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(2)));
    // MSVC has no line buffering (_IOLBF == _IOFBF); unbuffered keeps the last lines before a crash.
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    g_pump = new std::thread([read_fd] {
        std::string line;
        char buffer[1024];
        int count = 0;
        while ((count = _read(read_fd, buffer, sizeof(buffer))) > 0) {
            for (int i = 0; i < count; i++) {
                line.push_back(buffer[i]);
                if (buffer[i] == '\n' || line.size() >= 4096) {
                    write_line("stdio", line.data(), line.size());
                    line.clear();
                }
            }
        }
        if (!line.empty()) write_line("stdio", line.data(), line.size());
        _close(read_fd);
    });
    host_log("host", "stdio redirect active (Rust println!/eprintln!, C stderr, SDL/Mesa default loggers)");
}
} // namespace

bool host_log_open(const std::wstring& dir) {
    const auto path = std::filesystem::path(dir) / L"gopher64-host.log";
    std::error_code error;
    std::filesystem::rename(path, std::filesystem::path(dir) / L"gopher64-host.prev.log", error);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_file = _wfsopen(path.c_str(), L"w", _SH_DENYNO);
        if (g_file) g_file_handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(g_file)));
    }
    SetUnhandledExceptionFilter(crash_logger);
    host_log("host", std::string("log opened; previous run kept as gopher64-host.prev.log; debugger ") +
                         (IsDebuggerPresent() ? "attached (VS shows faults; the crash logger stays silent)"
                                              : "not attached (unhandled faults are logged as [crash])"));
    MEMORYSTATUSEX memory{sizeof(memory)};
    if (GlobalMemoryStatusEx(&memory))
        host_log("host", "memory: physical " + std::to_string(memory.ullTotalPhys >> 20) + " MiB, available " +
                             std::to_string(memory.ullAvailPhys >> 20) + " MiB; pid " +
                             std::to_string(GetCurrentProcessId()));
    redirect_stdio();
    return g_file != nullptr;
}

void host_log(const char* tag, const std::string& message) { write_line(tag, message.data(), message.size()); }

void host_log_apply_env_file(const std::wstring& path) {
    std::ifstream file{std::filesystem::path(path)};
    if (!file) {
        host_log("host", "env.txt not present (optional KEY=VALUE overrides, e.g. DZN_DEBUG=no16bit)");
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("\xef\xbb\xbf", 0) == 0) line.erase(0, 3);
        const auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] == '#') continue;
        const auto equals = line.find('=', start);
        if (equals == std::string::npos || equals == start) {
            host_log("host", "env.txt: ignoring line without '=': " + line);
            continue;
        }
        const std::string key = line.substr(start, line.find_last_not_of(" \t", equals - 1) - start + 1);
        const std::string value = line.substr(equals + 1);
        auto widen = [](const std::string& text) {
            std::wstring wide(text.size(), L'\0');
            wide.resize(MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(),
                                            static_cast<int>(wide.size())));
            return wide;
        };
        // _wputenv_s updates both the OS environment and the CRT copy that getenv() reads.
        const bool ok = _wputenv_s(widen(key).c_str(), widen(value).c_str()) == 0;
        host_log("host", std::string("env.txt: ") + key + "=" + value + (ok ? "" : " (_wputenv_s failed)"));
    }
}

void host_log_close() {
    host_log("host", "log closing");
    if (g_pump) {
        // Closing descriptors 1 and 2 closes the last write ends, so the pump drains and sees EOF.
        std::fflush(stdout);
        std::fflush(stderr);
        SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
        SetStdHandle(STD_ERROR_HANDLE, nullptr);
        _close(1);
        _close(2);
        g_pump->join();
        delete g_pump;
        g_pump = nullptr;
    }
    SetUnhandledExceptionFilter(nullptr);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_file_handle = INVALID_HANDLE_VALUE;
    if (g_file) std::fclose(g_file);
    g_file = nullptr;
}
