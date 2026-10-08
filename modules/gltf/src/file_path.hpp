#pragma once

#include <elf3d/core/result.h>

#include <filesystem>
#include <string_view>

namespace elf3d::gltf::file_path {

[[nodiscard]] Result<std::filesystem::path> from_utf8(std::string_view value) noexcept;

} // namespace elf3d::gltf::file_path
