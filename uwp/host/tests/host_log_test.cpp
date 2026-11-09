// PC test for uwp/host/host_log.cpp: every stdio path a UWP process uses must reach the log file.
#include "../host_log.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stdout, "FAIL line %d: %s\n", __LINE__, #cond);           \
            std::exit(1);                                                          \
        }                                                                          \
    } while (0)

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) return 2;
    const std::filesystem::path dir = argv[1];
    if (argc > 2 && std::wstring(argv[2]) == L"crash") {
        SetErrorMode(SEM_NOGPFAULTERRORBOX); // no WER dialog in the test
        host_log_open(dir.wstring());
        volatile int* null_pointer = nullptr;
        *null_pointer = 1; // unhandled access violation: the filter must log it
        return 0;
    }
    { std::ofstream(dir / L"gopher64-host.log") << "previous run\n"; }
    { std::ofstream(dir / L"env.txt") << "# comment\nG64_TEST_VAR = hello world\n=bad\nnoequals\n"; }

    CHECK(host_log_open(dir.wstring()));
    host_log_apply_env_file((dir / L"env.txt").wstring());
    char value[64] = {};
    CHECK(GetEnvironmentVariableA("G64_TEST_VAR", value, sizeof(value)) > 0);
    CHECK(std::string(value) == " hello world");

    host_log("host", "direct line");
    std::printf("crt stdout line\n");
    std::fprintf(stderr, "crt stderr line\n");
    const char rust_like[] = "win32 stdout line\n"; // Rust std writes via GetStdHandle + WriteFile
    DWORD written = 0;
    CHECK(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), rust_like, sizeof(rust_like) - 1, &written, nullptr));
    const char rust_err[] = "win32 stderr line\n";
    CHECK(WriteFile(GetStdHandle(STD_ERROR_HANDLE), rust_err, sizeof(rust_err) - 1, &written, nullptr));
    std::thread([] { std::fprintf(stderr, "worker thread stderr line\n"); }).join();
    std::fprintf(stderr, "no trailing newline");
    host_log_close();

    std::ifstream log(dir / L"gopher64-host.log");
    std::stringstream all;
    all << log.rdbuf();
    const std::string text = all.str();
    for (const char* expected :
         {"[host] direct line", "[stdio] crt stdout line", "[stdio] crt stderr line", "[stdio] win32 stdout line",
          "[stdio] win32 stderr line", "[stdio] worker thread stderr line", "[stdio] no trailing newline",
          "env.txt: G64_TEST_VAR= hello world", "env.txt: ignoring line without '=': =bad",
          "env.txt: ignoring line without '=': noequals", "stdio redirect active", "[host] log closing"}) {
        if (text.find(expected) == std::string::npos) {
            std::printf("FAIL: missing \"%s\"\n--- log ---\n%s", expected, text.c_str());
            return 1;
        }
    }
    std::ifstream prev(dir / L"gopher64-host.prev.log");
    std::string first;
    std::getline(prev, first);
    CHECK(first == "previous run");
    std::printf("PASS: host_log redirect, env.txt and rotation\n");
    return 0;
}
