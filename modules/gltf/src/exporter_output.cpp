module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module elf.gltf;

namespace elf3d::gltf::exporter_output {

using OutputFile = std::pair<std::filesystem::path, std::vector<std::byte>>;

namespace {

struct OutputArtifact {
    std::filesystem::path final_path;
    std::vector<std::byte> bytes;
    std::filesystem::path staged_path;
    std::filesystem::path backup_path;
    bool has_existing_file = false;
    bool backed_up = false;
    bool published = false;
};

[[nodiscard]] Result<void> write_file(const std::filesystem::path& path,
                                      std::span<const std::byte> bytes) {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        return Error{ErrorCode::source_file_write_failed, "Could not open output file for writing"};
    }
    if (!bytes.empty()) {
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    return stream ? Result<void>{}
                  : Result<void>{
                        Error{ErrorCode::source_file_write_failed, "Could not write output file"}};
}

[[nodiscard]] Result<std::filesystem::path>
available_sibling_path(const std::filesystem::path& final_path, std::string_view purpose) {
    for (std::uint32_t index = 0; index != 1024U; ++index) {
        const std::filesystem::path candidate =
            final_path.parent_path() / (final_path.filename().string() + ".elf3d-" +
                                        std::string{purpose} + "-" + std::to_string(index));
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

void remove_file_if_present(const std::filesystem::path& path) {
    if (!path.empty()) {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
}

[[nodiscard]] bool remove_published_output(OutputArtifact& artifact) {
    if (!artifact.published) {
        return true;
    }
    std::error_code error;
    std::filesystem::remove(artifact.final_path, error);
    return !error;
}

[[nodiscard]] bool restore_output_backup(OutputArtifact& artifact) {
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

[[nodiscard]] bool remove_staged_output(OutputArtifact& artifact) {
    if (artifact.staged_path.empty()) {
        return true;
    }
    std::error_code error;
    std::filesystem::remove(artifact.staged_path, error);
    return !error;
}

[[nodiscard]] Result<void> rollback_output(std::vector<OutputArtifact>& artifacts) {
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

[[nodiscard]] Error rollback_failure(std::vector<OutputArtifact>& artifacts, Error failure) {
    const Result<void> rolled_back = rollback_output(artifacts);
    return rolled_back ? std::move(failure) : rolled_back.error();
}

[[nodiscard]] Result<void> inspect_output_paths(std::vector<OutputArtifact>& artifacts) {
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

[[nodiscard]] Result<void> stage_output_files(std::vector<OutputArtifact>& artifacts) {
    for (OutputArtifact& artifact : artifacts) {
        const Result<std::filesystem::path> staged =
            available_sibling_path(artifact.final_path, "stage");
        if (!staged) {
            return staged.error();
        }
        artifact.staged_path = staged.value();
        if (const Result<void> written = write_file(artifact.staged_path, artifact.bytes);
            !written) {
            return written.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> backup_existing_outputs(std::vector<OutputArtifact>& artifacts) {
    for (OutputArtifact& artifact : artifacts) {
        if (!artifact.has_existing_file) {
            continue;
        }
        const Result<std::filesystem::path> backup =
            available_sibling_path(artifact.final_path, "backup");
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

[[nodiscard]] Result<void> publish_staged_outputs(std::vector<OutputArtifact>& artifacts) {
    for (OutputArtifact& artifact : artifacts) {
        std::error_code error;
        std::filesystem::rename(artifact.staged_path, artifact.final_path, error);
        if (error) {
            return Error{ErrorCode::source_file_write_failed,
                         "Could not publish a complete glTF output set"};
        }
        artifact.published = true;
    }
    return {};
}

void discard_output_backups(std::vector<OutputArtifact>& artifacts) {
    for (OutputArtifact& artifact : artifacts) {
        if (artifact.backed_up) {
            remove_file_if_present(artifact.backup_path);
            artifact.backed_up = false;
            artifact.backup_path.clear();
        }
    }
}

[[nodiscard]] Result<void> publish_artifacts(std::vector<OutputArtifact>& artifacts) {
    if (const Result<void> inspected = inspect_output_paths(artifacts); !inspected) {
        return inspected.error();
    }
    if (const Result<void> staged = stage_output_files(artifacts); !staged) {
        return rollback_failure(artifacts, staged.error());
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

Result<void> publish(std::vector<OutputFile>& files) {
    std::vector<OutputArtifact> artifacts;
    artifacts.reserve(files.size());
    for (OutputFile& file : files) {
        artifacts.push_back(OutputArtifact{std::move(file.first), std::move(file.second)});
    }
    return publish_artifacts(artifacts);
}

} // namespace elf3d::gltf::exporter_output
