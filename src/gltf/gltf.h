#pragma once

#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include <elf3d/core/diagnostics.h>
#include <filesystem>

namespace elf3d::gltf {

inline constexpr unsigned model_format_adapter_revision = 1;

[[nodiscard]] Result<LoadedDocument> load_document(const std::filesystem::path& path,
                                                   const ModelLoadOptions& options) noexcept;
[[nodiscard]] Result<ModelWriteReport> save_document(const std::filesystem::path& path,
                                                     DocumentView document,
                                                     const ModelWriteOptions& options) noexcept;

} // namespace elf3d::gltf
