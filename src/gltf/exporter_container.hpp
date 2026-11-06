#pragma once

#include <elf3d/core/result.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace elf3d::gltf::exporter_output {
class OutputFiles;
}
namespace elf3d::gltf::exporter_detail {
class BinaryBuilder;
// Explicit worker-side staging. Each advance borrows its inputs only during the
// call and writes at most one 64-KiB payload block plus fixed container framing.
class ExportGlbOutput final {
  public:
    ExportGlbOutput(ExportGlbOutput&&) noexcept = default;
    ExportGlbOutput& operator=(ExportGlbOutput&&) noexcept = default;
    ~ExportGlbOutput() noexcept = default;
    ExportGlbOutput(const ExportGlbOutput&) = delete;
    ExportGlbOutput& operator=(const ExportGlbOutput&) = delete;
    [[nodiscard]] static Result<ExportGlbOutput> create(const std::filesystem::path& path,
                                                        std::size_t json_size,
                                                        std::size_t binary_size,
                                                        exporter_output::OutputFiles& files);
    [[nodiscard]] Result<void> advance(std::string_view json, BinaryBuilder& binary);
    [[nodiscard]] Result<void> discard();
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] std::size_t written_bytes() const noexcept;

  private:
    enum class Phase { json, binary_header, binary, finish, complete, disposed };
    ExportGlbOutput() = default;
    [[nodiscard]] Result<void> append_json(std::string_view json);
    [[nodiscard]] Result<void> append_binary_header();
    [[nodiscard]] Result<void> append_binary(BinaryBuilder& binary);
    std::ofstream stream_;
    std::size_t json_size_ = 0;
    std::size_t binary_size_ = 0;
    std::size_t offset_ = 0;
    std::size_t written_ = 0;
    Phase phase_ = Phase::json;
};
[[nodiscard]] Result<void> stage_glb(const std::filesystem::path& path, std::string_view json,
                                     BinaryBuilder& binary, exporter_output::OutputFiles& files);
} // namespace elf3d::gltf::exporter_detail
