module elf.core;

namespace elf3d::core {
namespace {

constexpr VersionData elf3d_version{0, 7, 1};
constexpr char elf3d_version_string[] = "0.7.1";

} // namespace

VersionData version_data() noexcept {
    return elf3d_version;
}

const char *version_string() noexcept {
    return elf3d_version_string;
}

} // namespace elf3d::core
