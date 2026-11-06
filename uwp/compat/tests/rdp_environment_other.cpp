#include "../rdp_environment.h"

bool configure_in_other_translation_unit(unsigned size, bool supports_control) {
    return g64_uwp_configure_rdp_subgroup(size, supports_control);
}
