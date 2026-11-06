#include <gltf/gltf.h>

#include "exporter_output.hpp"

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
namespace elf3d::gltf::exporter_output {

namespace {

[[nodiscard]] Result<void> write_file(const std::filesystem::path& path,
                                      std::span<const std::byte> bytes)
{
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        return Error{ErrorCode::source_file_write_failed, "Could not open output file for writing"};
    }
    const auto written = write_bytes(stream, bytes);
    auto closed = close_file(stream);
    if (!written) {
        return written.error();
    }
    return closed;
}

[[nodiscard]] Result<std::filesystem::path>
find_sibling_path(const std::filesystem::path& final_path, std::string_view purpose)
{
    for (std::uint32_t index = 0; index != 1024U; ++index) {
        auto sibling_name = final_path.filename();
        sibling_name += ".elf3d-" + std::string{purpose} + "-" + std::to_string(index);
        const std::filesystem::path candidate = final_path.parent_path() / sibling_name;
        std::error_code error;
        const bool exists = std::filesystem::exists(candidate, error);
        if (error) {
            return Error{ErrorCode::source_file_write_failed,
                         "Could not inspect a staged glTF output path"};
        }
        if (!exists) {
            return candidate;
        }
    }
    return Error{ErrorCode::source_file_write_failed,
                 "Could not reserve a staged glTF output path"};
}

void remove_file_if_present(const std::filesystem::path& path)
{
    if (!path.empty()) {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
}

[[nodiscard]] bool remove_published_output(OutputArtifact& artifact)
{
    if (!artifact.published) {
        return true;
    }

    std::error_code error;
    std::filesystem::remove(artifact.final_path, error);
    return !error;
}

[[nodiscard]] bool restore_output_backup(OutputArtifact& artifact)
{
    if (!artifact.backed_up) {
        return true;
    }

    std::error_code error;
    std::filesystem::rename(artifact.backup_path, artifact.final_path, error);
    if (error) {
        return false;
    }
    artifact.backed_up = false;
    artifact.backup_path.clear();
    return true;
}

[[nodiscard]] bool remove_staged_output(OutputArtifact& artifact)
{
    if (artifact.staged_path.empty()) {
        return true;
    }

    std::error_code error;
    std::filesystem::remove(artifact.staged_path, error);
    return !error;
}

[[nodiscard]] Result<void> rollback_output(std::vector<OutputArtifact>& artifacts)
{
    bool complete = true;
    for (OutputArtifact& artifact : artifacts) {
        const bool removed = remove_published_output(artifact);
        complete = removed && complete;
    }
    for (OutputArtifact& artifact : artifacts) {
        const bool restored = restore_output_backup(artifact);
        const bool removed = remove_staged_output(artifact);
        complete = restored && removed && complete;
    }
    if (!complete) {
        return Error{ErrorCode::source_file_write_failed,
                     "Could not restore every pre-existing glTF output; recovery backups were "
                     "retained"};
    }
    return {};
}

[[nodiscard]] Error rollback_failure(std::vector<OutputArtifact>& artifacts, Error failure)
{
    const Result<void> rolled_back = rollback_output(artifacts);
    return rolled_back ? std::move(failure) : rolled_back.error();
}

[[nodiscard]] Result<void> inspect_output_paths(std::vector<OutputArtifact>& artifacts)
{
    for (OutputArtifact& artifact : artifacts) {
        std::error_code error;
        const bool exists = std::filesystem::exists(artifact.final_path, error);
        if (error || (exists && !std::filesystem::is_regular_file(artifact.final_path, error))) {
            return Error{ErrorCode::source_file_write_failed,
                         "A glTF output path cannot be replaced transactionally"};
        }
        artifact.has_existing_file = exists;
    }
    return {};
}

[[nodiscard]] Result<void> backup_existing_outputs(std::vector<OutputArtifact>& artifacts)
{
    for (OutputArtifact& artifact : artifacts) {
        if (!artifact.has_existing_file) {
            continue;
        }

        const Result<std::filesystem::path> backup =
            find_sibling_path(artifact.final_path, "backup");
        if (!backup) {
            return backup.error();
        }
        artifact.backup_path = backup.value();
        std::error_code error;
        std::filesystem::rename(artifact.final_path, artifact.backup_path, error);
        if (error) {
            return Error{ErrorCode::source_file_write_failed,
                         "Could not stage an existing glTF output for replacement"};
        }
        artifact.backed_up = true;
    }
    return {};
}

[[nodiscard]] Result<void> publish_staged_outputs(std::vector<OutputArtifact>& artifacts)
{
    for (OutputArtifact& artifact : artifacts) {
        std::error_code error;
        std::filesystem::rename(artifact.staged_path, artifact.final_path, error);
        if (error) {
            return Error{ErrorCode::source_file_write_failed,
                         "Could not publish a complete glTF output set"};
        }
        artifact.published = true;
        artifact.staged_path.clear();
    }
    return {};
}

void discard_output_backups(std::vector<OutputArtifact>& artifacts)
{
    for (OutputArtifact& artifact : artifacts) {
        if (artifact.backed_up) {
            remove_file_if_present(artifact.backup_path);
            artifact.backed_up = false;
            artifact.backup_path.clear();
        }
    }
}

[[nodiscard]] Result<void> publish_artifacts(std::vector<OutputArtifact>& artifacts)
{
    if (const Result<void> inspected = inspect_output_paths(artifacts); !inspected) {
        return inspected.error();
    }
    if (const Result<void> backed_up = backup_existing_outputs(artifacts); !backed_up) {
        return rollback_failure(artifacts, backed_up.error());
    }
    if (const Result<void> published = publish_staged_outputs(artifacts); !published) {
        return rollback_failure(artifacts, published.error());
    }

    discard_output_backups(artifacts);
    return {};
}

} // namespace

Result<std::filesystem::path> available_sibling_path(const std::filesystem::path& final_path,
                                                     std::string_view purpose)
{
    return find_sibling_path(final_path, purpose);
}

Result<void> write_bytes(std::ofstream& stream, std::span<const std::byte> bytes)
{
    while (!bytes.empty()) {
        const auto count = std::min(bytes.size(), transfer_chunk_size);
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(count));
        if (!stream) {
            return Error{ErrorCode::source_file_write_failed,
                         "Could not write staged output block"};
        }
        bytes = bytes.subspan(count);
    }
    return {};
}

Result<void> close_file(std::ofstream& stream)
{
    stream.flush();
    stream.close();
    return stream ? Result<void>{}
                  : Result<void>{Error{ErrorCode::source_file_write_failed,
                                       "Could not finish staged output file"}};
}

OutputFiles::~OutputFiles() noexcept
{
    (void)discard();
}

Result<std::filesystem::path> OutputFiles::stage(const std::filesystem::path& final_path)
{
    auto path = find_sibling_path(final_path, "stage");
    if (!path) {
        return path.error();
    }
    artifacts_.push_back(OutputArtifact{final_path, path.value()});
    return std::move(path).value();
}

void OutputFiles::adopt(std::filesystem::path final_path, std::filesystem::path staged_path)
{
    artifacts_.push_back(OutputArtifact{std::move(final_path), std::move(staged_path)});
}

Result<void> OutputFiles::stage_bytes(const std::filesystem::path& final_path,
                                      std::span<const std::byte> bytes)
{
    const auto staged = stage(final_path);
    return staged ? write_file(staged.value(), bytes) : Result<void>{staged.error()};
}

Result<void> OutputFiles::publish()
{
    return publish_artifacts(artifacts_);
}

Result<void> OutputFiles::discard() noexcept
{
    try {
        bool complete = true;
        for (auto& artifact : artifacts_) {
            const bool removed = remove_staged_output(artifact);
            complete = removed && complete;
        }
        if (!complete) {
            return Error{ErrorCode::source_file_write_failed,
                         "Could not discard staged glTF outputs; temporary files were retained"};
        }
        artifacts_.clear();
        return {};
    } catch (const std::bad_alloc&) {
        fatal_error("glTF output cleanup allocation failed");
    } catch (...) {
        fatal_error("Unexpected glTF output cleanup exception");
    }
}

} // namespace elf3d::gltf::exporter_output
