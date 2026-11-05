#include <SDL3/SDL.h>
#include <cstdlib>
#include <new>

static unsigned allocated = 0, freed = 0, errors = 0;
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    void* ptr = std::malloc(size);
    if (ptr) ++allocated;
    return ptr;
}
void operator delete(void* ptr) noexcept { if (ptr) ++freed; std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { if (ptr) ++freed; std::free(ptr); }

static bool fake_schedule(SDL_MainThreadCallback, void*, bool) { return false; }
#define SDL_RunOnMainThread fake_schedule
#include "../compat/g64_gamepad_event.h"
#undef SDL_RunOnMainThread

extern "C" void gopher64_uwp_log_native(const char*) { ++errors; }
static void SDLCALL callback(void*) {}

int main() {
    g64_uwp_queue_gamepad_event(42, callback);
    return allocated == 1 && freed == 1 && errors == 1 ? 0 : 1;
}
