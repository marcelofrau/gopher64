#include "../rdp_environment.h"
#include <cassert>

#ifdef NDEBUG
#error This test requires assertions to be enabled.
#endif

int main() {
    assert(SetEnvironmentVariableW(L"PARALLEL_RDP_UBERSHADER", L"0"));
    assert(std::strcmp(g64_uwp_rdp_getenv("PARALLEL_RDP_UBERSHADER"), "1") == 0);
    assert(SetEnvironmentVariableW(L"PARALLEL_RDP_ALLOW_EXTERNAL_HOST", L"1"));
    assert(std::strcmp(g64_uwp_rdp_getenv("PARALLEL_RDP_ALLOW_EXTERNAL_HOST"), "0") == 0);
    assert(SetEnvironmentVariableW(L"G64_RDP_ENV_TEST", L"123"));
    assert(std::strcmp(g64_uwp_rdp_getenv("G64_RDP_ENV_TEST"), "123") == 0);
}
