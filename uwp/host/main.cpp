#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <windows.h>
#include <ppltasks.h>
#include <share.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <utility>
#include <mutex>
#include <sstream>
#include <string>

int g64_dozen_probe(void (*log)(const char*), bool enable_8bit, void* core_window);
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

struct PickerState {
    std::mutex mutex;
    bool pending = false;
    bool done = false;
    std::string path;
    std::string error;
    std::string progress;
};

constexpr bool picker_on_launch(bool autoboot, bool probe, bool after_game) {
    return !autoboot && !probe && !after_game;
}
static_assert(picker_on_launch(false, false, false));
static_assert(!picker_on_launch(true, false, false));
static_assert(!picker_on_launch(false, true, false));
static_assert(!picker_on_launch(false, false, true));

void choose_rom(const std::shared_ptr<PickerState>& state) {
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->pending) return;
        state->pending = true;
        state->done = false;
        state->path.clear();
        state->error.clear();
        state->progress.clear();
    }
    try {
        auto picker = ref new Windows::Storage::Pickers::FileOpenPicker();
        for (auto extension : {L".n64", L".v64", L".z64", L".bin", L".zip", L".7z"}) {
            picker->FileTypeFilter->Append(ref new Platform::String(extension));
        }
        concurrency::create_task(picker->PickSingleFileAsync()).then(
            [state](Windows::Storage::StorageFile^ file) -> concurrency::task<Windows::Storage::StorageFile^> {
                if (!file) return concurrency::task_from_result<Windows::Storage::StorageFile^>(nullptr);
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    state->progress = "Copying selected ROM to the app staging file...";
                }
                auto root = Windows::Storage::ApplicationData::Current->LocalFolder;
                return concurrency::create_task(root->CreateFolderAsync(
                    L"gopher64-staging", Windows::Storage::CreationCollisionOption::OpenIfExists)).then(
                    [file](Windows::Storage::StorageFolder^ folder) -> concurrency::task<Windows::Storage::StorageFile^> {
                        auto destination = std::filesystem::path(folder->Path->Data()) / L"gopher64-staged.rom";
                        if (_wcsicmp(file->Path->Data(), destination.c_str()) == 0)
                            return concurrency::task_from_result(file);
                        return concurrency::create_task(file->CopyAsync(
                            folder, L"gopher64-staged.rom", Windows::Storage::NameCollisionOption::ReplaceExisting));
                    });
            }).then(
            [state](concurrency::task<Windows::Storage::StorageFile^> result) {
                std::string path;
                std::string error;
                try {
                    auto file = result.get();
                    if (file) {
                        auto value = file->Path;
                        path = utf8(std::wstring(value->Data(), value->Length()));
                        if (path.empty()) error = "The selected file has no usable path.";
                    }
                } catch (Platform::Exception^ exception) {
                    auto message = exception->Message;
                    error = utf8(std::wstring(message->Data(), message->Length()));
                    if (error.empty()) error = "Windows could not complete the file picker.";
                } catch (const std::exception& exception) {
                    error = exception.what();
                }
                std::lock_guard<std::mutex> lock(state->mutex);
                state->path = std::move(path);
                state->error = std::move(error);
                state->pending = false;
                state->done = true;
            }, concurrency::task_continuation_context::use_current());
    } catch (Platform::Exception^ exception) {
        auto message = exception->Message;
        std::lock_guard<std::mutex> lock(state->mutex);
        state->error = utf8(std::wstring(message->Data(), message->Length()));
        if (state->error.empty()) state->error = "Windows could not open the file picker.";
        state->pending = false;
        state->done = true;
    }
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

    std::error_code probe_error;
    if (std::filesystem::exists(root_path / L"p3-dozen-probe.txt", probe_error)) {
        log("Dozen probe starting");
        std::ifstream marker(root_path / L"p3-dozen-probe.txt");
        std::string mode;
        marker >> mode;
        SDL_Window* window = nullptr;
        void* core_window = nullptr;
        if (mode == "surface") {
            if (!SDL_Init(SDL_INIT_VIDEO) || !(window = SDL_CreateWindow("Dozen surface probe", 640, 480, 0))) {
                log(std::string("Dozen surface probe SDL initialization failed: ") + SDL_GetError());
                SDL_Quit();
                close_log();
                return 1;
            }
            core_window = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WINRT_WINDOW_POINTER, nullptr);
            if (!core_window) {
                log("Dozen surface probe: SDL did not provide a CoreWindow");
                SDL_DestroyWindow(window);
                SDL_Quit();
                close_log();
                return 1;
            }
        }
        const int probe_result = g64_dozen_probe([](const char* message) { log(std::string("[dozen] ") + message); },
                                                 mode == "8bit" || mode == "surface", core_window);
        if (window) {
            SDL_DestroyWindow(window);
            SDL_Quit();
        }
        log("Dozen probe finished with code " + std::to_string(probe_result));
        close_log();
        return probe_result;
    }
    if (probe_error) log("Could not check Dozen probe marker: " + probe_error.message());

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
    std::string status = "Select a ROM to start.";
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
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "1");
        SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
        if (!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_WGI, false)) {
            log("Input: Windows Gaming Input is disabled by an SDL hint override");
        }
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
        const auto picker = std::make_shared<PickerState>();
        bool open_picker = picker_on_launch(autoboot, auto_probe, after_game);
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
            bool picker_pending;
            {
                std::lock_guard<std::mutex> lock(picker->mutex);
                picker_pending = picker->pending;
                if (!picker->progress.empty()) {
                    status = std::exchange(picker->progress, {});
                    log(status);
                }
                if (picker->done) {
                    picker->done = false;
                    if (!picker->error.empty()) status = "ROM picker failed: " + picker->error;
                    else if (picker->path.empty()) status = "No ROM selected. Select a ROM to try again.";
                    else {
                        rom = picker->path;
                        autoboot = true;
                        status = "ROM copied to local staging; starting.";
                    }
                    log(status);
                }
            }
            ImGui::BeginDisabled(picker_pending || autoboot);
            if (ImGui::Button("Select ROM")) open_picker = true;
            ImGui::EndDisabled();
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
            if (open_picker && !first_frame && running) {
                open_picker = false;
                status = "Waiting for ROM selection...";
                log("Opening ROM file picker");
                choose_rom(picker);
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
