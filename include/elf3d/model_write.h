#pragma once

#include <elf3d/model.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace elf3d {
namespace model::detail {
class WriteSdkAccess;
}

// Process-local request value, meaningful only for its captured source/revision.
// It contains coordinates and identity, never a Document observer or payload.
class ModelWriteInputRequest final {
  public:
    ModelWriteInputRequest() noexcept = default;
    bool operator==(const ModelWriteInputRequest&) const noexcept = default;

  private:
    friend class model::detail::WriteSdkAccess;
    std::array<std::uint64_t, 12> fields_{};
};
// Minimum destination size; a larger destination permits bounded batching.
inline constexpr std::size_t model_write_packet_capacity = 64 * 1024 + 256;
inline constexpr std::size_t model_write_packet_limit = 2 * 1024 * 1024;

// Small copyable source capability. Creation captures identity, revision and
// counts only. Each read borrows the current Document on its owning thread and
// writes a bounded owned packet (up to 64 metadata/stream advances per call).
// Source replacement/mutation rejects reads.
// Copies retain no Document, text, geometry or image owner.
class ModelWriteSource final {
  public:
    ModelWriteSource() noexcept = default;
    [[nodiscard]] static Result<ModelWriteSource> create(DocumentView document) noexcept;
    [[nodiscard]] bool matches_document(DocumentView document) const noexcept;
    [[nodiscard]] std::uint64_t document_revision() const noexcept;
    [[nodiscard]] Result<std::size_t> read(DocumentView document, ModelWriteInputRequest request,
                                           std::span<std::byte> destination) const noexcept;

  private:
    friend class model::detail::WriteSdkAccess;
    std::uintptr_t owner_domain_ = 0;
    std::uint64_t owner_sequence_ = 0;
    std::uint64_t revision_ = 0;
    std::array<std::size_t, 12> counts_{};
    std::optional<DocumentSceneId> default_scene_;
    std::size_t required_extensions_ = 0;
    bool metadata_stale_ = false;
};

enum class ModelWritePhase {
    metadata,
    validating,
    geometry,
    images,
    image_output,
    json,
    staging,
    commit,
    cleanup,
    completed,
    cancelled,
    failed
};
struct ModelWriteProgress {
    ModelWritePhase phase = ModelWritePhase::metadata;
    std::size_t completed = 0;
    std::size_t total = 0; // Zero means indeterminate, not zero remaining work.
};
struct ModelWriteAdvanceOptions {
    bool cancel_requested = false;
};
struct ModelWriteOutcome {
    bool committed = false;
    bool cancelled = false;
    std::optional<Error> error;
    std::optional<Error> cleanup_error;
    ModelWriteReport report;
};

// Explicit CPU-owned operation, driven by the caller without hidden threads.
// Supply packets requested by input_request, otherwise call advance. All reads,
// validation, codecs, serialization, file IO and cleanup run on the calling
// owner. No live Document is retained or borrowed asynchronously. Independent
// source reading and writing owners exchange owned packet bytes only.
class ModelFileWriter final {
  public:
    ModelFileWriter() noexcept;
    ~ModelFileWriter() noexcept;
    ModelFileWriter(ModelFileWriter&&) noexcept;
    ModelFileWriter& operator=(ModelFileWriter&&) noexcept;
    ModelFileWriter(const ModelFileWriter&) = delete;
    ModelFileWriter& operator=(const ModelFileWriter&) = delete;
    [[nodiscard]] static Result<ModelFileWriter>
    create(std::string_view path_utf8, ModelWriteSource source,
           const ModelWriteOptions& options = {}) noexcept;
    [[nodiscard]] std::optional<ModelWriteInputRequest> input_request() const noexcept;
    [[nodiscard]] Result<void> provide(std::span<const std::byte> packet) noexcept;
    [[nodiscard]] Result<void> reject_input(Error error) noexcept;
    [[nodiscard]] Result<void> advance(const ModelWriteAdvanceOptions& options = {}) noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] ModelWriteProgress progress() const noexcept;
    [[nodiscard]] Result<ModelWriteOutcome> take_outcome() noexcept;
    // Explicit cleanup before destruction. Does not undo committed output;
    // recovery backups survive failed rollback. A commit entered by advance
    // finishes/recoveries synchronously; late cancellation keeps its real result.
    [[nodiscard]] Result<void> discard() noexcept;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace elf3d
