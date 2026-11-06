#pragma once

#include "exporter_binary_spool.hpp"

#include <elf3d/core/result.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace elf3d::gltf::exporter_detail {
// Incremental RGBA8 PNG output. Input and compression buffers remain bounded
// independently of dimensions. No row/image array, Document or output is retained.
class ExportPngEncoder final {
  public:
    ~ExportPngEncoder() noexcept;
    ExportPngEncoder(ExportPngEncoder&&) noexcept;
    ExportPngEncoder& operator=(ExportPngEncoder&&) noexcept;
    ExportPngEncoder(const ExportPngEncoder&) = delete;
    ExportPngEncoder& operator=(const ExportPngEncoder&) = delete;
    [[nodiscard]] static Result<ExportPngEncoder> create(std::uint32_t width, std::uint32_t height,
                                                         ExportBinarySpool& output);
    [[nodiscard]] Result<void> accept(std::span<const std::byte> pixels, ExportBinarySpool& output);
    [[nodiscard]] Result<void> finish(ExportBinarySpool& output);

  private:
    ExportPngEncoder() noexcept;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace elf3d::gltf::exporter_detail
