#pragma once

#include <elf3d/core/result.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

namespace elf3d::gltf::exporter_output {
inline constexpr std::size_t transfer_chunk_size = 2 * 1024 * 1024;

struct OutputArtifact {
    std::filesystem::path final_path;
    std::filesystem::path staged_path;
    std::filesystem::path backup_path;
    bool has_existing_file = false;
    bool backed_up = false;
    bool published = false;
};

// Owns staged output paths and recovery state, never complete file buffers.
// Normal exits explicitly discard remaining staging. Recovery backups survive
// a failed rollback; neither discard nor destruction removes those backups.
class OutputFiles final {
  public:
    OutputFiles() = default;
    ~OutputFiles() noexcept;
    OutputFiles(const OutputFiles&) = delete;
    OutputFiles& operator=(const OutputFiles&) = delete;
    [[nodiscard]] Result<std::filesystem::path> stage(const std::filesystem::path& final_path);
    void adopt(std::filesystem::path final_path, std::filesystem::path staged_path);
    [[nodiscard]] Result<void> stage_bytes(const std::filesystem::path& final_path,
                                           std::span<const std::byte> bytes);
    [[nodiscard]] Result<void> publish();
    [[nodiscard]] Result<void> discard() noexcept;

  private:
    std::vector<OutputArtifact> artifacts_;
};

[[nodiscard]] Result<std::filesystem::path>
available_sibling_path(const std::filesystem::path& final_path, std::string_view purpose);
[[nodiscard]] Result<void> write_bytes(std::ofstream& stream, std::span<const std::byte> bytes);
[[nodiscard]] Result<void> close_file(std::ofstream& stream);
} // namespace elf3d::gltf::exporter_output
