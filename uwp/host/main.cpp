#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <windows.h>
#include <share.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

extern "C" void gopher64_uwp_set_root(const char* root);
extern "C" void gopher64_uwp_set_log_callback(void (*callback)(const char*));
extern "C" int gopher64_uwp_main(int argc, char** argv);

namespace {
FILE* log_file = nullptr;
std::mutex log_mutex;
std::string last_rust_error;

void log(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char timestamp[32];
    std::snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                  now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    const std::string line = std::string(timestamp) + " [host] " + message + "\n";
    OutputDebugStringA(line.c_str());
    if (log_file) {
        std::fputs(line.c_str(), log_file);
        std::fflush(log_file);
    }
}

void rust_log(const char* message) noexcept {
    try {
        std::istringstream lines(message ? message : "(null)");
        std::string line;
        while (std::getline(lines, line)) {
            if (line.rfind("error running game: ", 0) == 0) {
                std::lock_guard<std::mutex> lock(log_mutex);
                last_rust_error = line.substr(20);
            }
            log("[rust] " + line);
        }
    } catch (...) {
        OutputDebugStringA("[gopher64] failed to write a Rust log message\n");
    }
}

void close_log() {
    gopher64_uwp_set_log_callback(nullptr);
    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_file) std::fclose(log_file);
    log_file = nullptr;
}

std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                   nullptr, 0, nullptr, nullptr);
    if (!size) return {};
    std::string result(size, '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                             result.data(), size, nullptr, nullptr)) return {};
    return result;
}

int run_rust(const std::string& rom) {
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        last_rust_error.clear();
    }
    std::vector<char> game(rom.begin(), rom.end());
    game.push_back('\0');
    char executable[] = "gopher64";
    char* args[] = {executable, game.data()};
    log("Calling Rust with ROM: " + rom);
    const int result = gopher64_uwp_main(2, args);
    log("Rust returned " + std::to_string(result));
    return result;
}

std::string run_probe(const std::filesystem::path& root) {
    const auto missing = root / L"__gopher64_p2_probe__" / L"missing.n64";
    std::error_code error;
    if (std::filesystem::exists(missing, error) || error) {
        return "Cannot safely run probe: test path exists or cannot be checked.";
    }
    const int result = run_rust(utf8(missing.wstring()));
    return "Rust probe returned " + std::to_string(result) + " (expected 1: ROM not found).";
}
}

int main(int, char**) {
    std::wstring root;
    try {
        auto path = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
        root.assign(path->Data(), path->Length());
    } catch (Platform::Exception^ error) {
        OutputDebugStringW(error->Message->Data());
        return 1;
    }
    const auto root_utf8 = utf8(root);
    if (root_utf8.empty()) {
        OutputDebugStringA("Gopher64: could not convert LocalState path to UTF-8\n");
        return 1;
    }
    const auto root_path = std::filesystem::path(root);
    const auto log_path = root_path / L"gopher64-host.log";
    log_file = _wfsopen(log_path.c_str(), L"a", _SH_DENYNO);
    log("Starting Gopher64 UWP; LocalState: " + root_utf8);
    gopher64_uwp_set_log_callback(rust_log);
    gopher64_uwp_set_root(root_utf8.c_str());

    std::string rom;
    const auto config_path = root_path / L"autoboot.txt";
    std::ifstream config(config_path, std::ios::binary);
    const bool config_open = config.is_open();
    if (config) {
        std::getline(config, rom);
        if (!rom.empty() && rom.back() == '\r') rom.pop_back();
        if (rom.compare(0, 3, "\xef\xbb\xbf") == 0) rom.erase(0, 3);
        config.close();
    }
    std::string status = "Ready. Create autoboot.txt in LocalState with the UTF-8 path to a ROM.";
    std::error_code config_error;
    const bool config_exists = std::filesystem::exists(config_path, config_error);
    if (config_error) status = "Cannot check autoboot.txt: " + config_error.message();
    else if (config_exists && !config_open) status = "Cannot open autoboot.txt.";
    else if (config_open && rom.empty()) status = "autoboot.txt is empty.";
    bool autoboot = false;
    if (rom.find('\0') != std::string::npos) {
        status = "Autoboot path contains a null byte.";
    } else if (!rom.empty()) {
        std::error_code error;
        try {
            const auto rom_path = std::filesystem::u8path(rom);
            if (!rom_path.is_absolute()) {
                status = "Autoboot path must be absolute.";
            } else if (!std::filesystem::is_regular_file(rom_path, error)) {
                status = "Autoboot ROM is unavailable: " + rom;
                if (error) status += " (" + error.message() + ")";
            } else {
                autoboot = true;
                status = "Autoboot ROM found; starting after the first frame.";
            }
        } catch (const std::exception& exception) {
            status = std::string("Invalid ROM path: ") + exception.what();
        }
    }
    log(status);
    if (!log_file) status += " Host log file could not be opened; see debugger output.";

    int result = 0;
    bool after_game = false;
    for (;;) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            log(std::string("SDL_Init failed: ") + SDL_GetError());
            close_log();
            return 1;
        }
        SDL_Window* window = SDL_CreateWindow("Gopher64", 1280, 720, SDL_WINDOW_RESIZABLE);
        SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
        if (!window || !renderer) {
            log(std::string("SDL window/renderer failed: ") + SDL_GetError());
            if (renderer) SDL_DestroyRenderer(renderer);
            if (window) SDL_DestroyWindow(window);
            SDL_Quit();
            close_log();
            return 1;
        }
        log(std::string("SDL renderer: ") + SDL_GetRendererName(renderer));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
        ImGui::StyleColorsDark();
        const bool imgui_platform = ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
        const bool imgui_renderer = imgui_platform && ImGui_ImplSDLRenderer3_Init(renderer);
        if (!imgui_renderer) {
            log("ImGui initialization failed");
            if (imgui_platform) ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            close_log();
            return 1;
        }
        log("Status screen initialized");

        bool running = true;
        bool first_frame = true;
        bool launch = false;
        bool frontend_failed = false;
        bool probe_done = false;
        std::error_code probe_error;
        const bool auto_probe = !autoboot && std::filesystem::is_regular_file(root_path / L"p2-probe.txt", probe_error);
        std::error_code debug_error;
        std::error_code test_error;
        const bool auto_close_test = after_game && std::filesystem::is_regular_file(root_path / L"p2-rom-test.txt", test_error);
        if (!after_game && std::filesystem::is_regular_file(root_path / L"p2-debug-wait.txt", debug_error)) {
            log("Diagnostic mode: waiting 8 seconds for debugger attachment");
            SDL_Delay(8000);
        }
        log("Entering frontend loop");
        while (running) {
            SDL_Event event;
            int traced_events = 0;
            if (first_frame) log("First frame: polling events; window flags=" + std::to_string(SDL_GetWindowFlags(window)));
            while (SDL_PollEvent(&event)) {
                if (first_frame && traced_events++ < 8) log("First frame: SDL event " + std::to_string(event.type));
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
            }
            if (first_frame) log("First frame: beginning ImGui frame");
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            if (first_frame) log("First frame: building status window");
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::Begin("Gopher64 UWP", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
            ImGui::TextUnformatted("Gopher64 - UWP development build");
            ImGui::Separator();
            ImGui::TextWrapped("%s", status.c_str());
            ImGui::TextWrapped("LocalState: %s", root_utf8.c_str());
            ImGui::TextUnformatted("Host log: gopher64-host.log");
            if (!probe_done && ImGui::Button("Test Rust entry (missing ROM)")) {
                status = run_probe(root_path);
                log(status);
                probe_done = true;
            }
            ImGui::End();
            ImGui::Render();
            if (first_frame) log("First frame: rendering draw data");
            SDL_SetRenderDrawColor(renderer, 20, 22, 28, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
            if (first_frame) log("First frame: presenting");
            if (!SDL_RenderPresent(renderer)) {
                log(std::string("SDL_RenderPresent failed: ") + SDL_GetError());
                frontend_failed = true;
                running = false;
            } else if (first_frame) {
                log("First status frame presented");
                if (after_game) log("Restored status frame presented");
                first_frame = false;
            }
            if (auto_close_test && !first_frame) {
                running = false;
            }
            if (auto_probe && !first_frame && !probe_done && running) {
                status = run_probe(root_path);
                log(status);
                probe_done = true;
                running = false;
            }
            if (autoboot && running) {
                launch = true;
                running = false;
            }
            SDL_Delay(10);
        }

        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        log("Frontend shut down");
        if (frontend_failed) { result = 1; break; }
        if (!launch) break;
        result = run_rust(rom);
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            if (result == 0) status = "Game session ended.";
            else if (last_rust_error.empty()) status = "Game failed with code " + std::to_string(result) + ". See the host log.";
            else status = "Game failed: " + last_rust_error;
        }
        autoboot = false;
        after_game = true;
        log("Restoring frontend: " + status);
    }
    log("Host exiting with code " + std::to_string(result));
    close_log();
    return result;
}
