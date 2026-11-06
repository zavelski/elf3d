#pragma once

#include <elf3d/core/result.h>

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <span>

namespace elf3d::gltf::exporter_detail {
// The generated binary section lives in one owned sibling file. Buffering is
// bounded independently of the model/container size. No Document is retained.
class ExportBinarySpool final {
  public:
    ExportBinarySpool() noexcept;
    ~ExportBinarySpool() noexcept;
    ExportBinarySpool(ExportBinarySpool&&) noexcept;
    ExportBinarySpool& operator=(ExportBinarySpool&&) noexcept;
    ExportBinarySpool(const ExportBinarySpool&) = delete;
    ExportBinarySpool& operator=(const ExportBinarySpool&) = delete;
    [[nodiscard]] Result<void> open(const std::filesystem::path& target);
    void append_byte(std::byte value);
    void append(std::span<const std::byte> bytes);
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] Result<void> status() const noexcept;
    // Reading seals output. Each owned destination is at most the spool buffer;
    // repeated reads use one file cursor, never a whole-file byte vector.
    [[nodiscard]] Result<void> read(std::size_t offset, std::span<std::byte> destination);
    [[nodiscard]] Result<void> copy_to(std::ostream& destination);
    [[nodiscard]] Result<std::filesystem::path> take_path();
    [[nodiscard]] Result<void> discard() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace elf3d::gltf::exporter_detail
