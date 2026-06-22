#include <elf3d/core/version_data.h>

namespace elf3d::core {
namespace {

constexpr VersionData elf3d_version{0, 1, 0};
constexpr char elf3d_version_string[] = "0.1.0";

} // namespace

VersionData version_data() noexcept {
    return elf3d_version;
}

const char *version_string() noexcept {
    return elf3d_version_string;
}

} // namespace elf3d::core
