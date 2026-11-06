#pragma once

#include "exporter_internal.hpp"

#include <model/document_write_source.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>

namespace elf3d::gltf::exporter_detail {
struct ExportImageMemory {
    std::size_t encoded_candidate = 0;
    std::size_t decoded_candidate = 0;
    std::size_t peak_candidates = 0;
};
// Receives owned current-image packets. Only one bounded codec candidate may
// exist; canonical Document/Scene views and image payloads are never retained.
class ExportImageInput final {
  public:
    ~ExportImageInput() noexcept;
    ExportImageInput(ExportImageInput&&) noexcept;
    ExportImageInput& operator=(ExportImageInput&&) noexcept;
    ExportImageInput(const ExportImageInput&) = delete;
    ExportImageInput& operator=(const ExportImageInput&) = delete;
    [[nodiscard]] static Result<ExportImageInput> create(model::detail::WriteSourceRecord source,
                                                         const ExportData& metadata,
                                                         const OutputPath& target);
    [[nodiscard]] std::optional<model::detail::WriteSourceDataRequest> request() const noexcept;
    [[nodiscard]] Result<void> accept(model::detail::WriteSourceDataRequest request,
                                      model::detail::WriteSourceChunk chunk,
                                      std::span<const std::byte> bytes);
    // Decode is an explicit CPU phase, so its caller can publish indeterminate
    // progress and cancellation acknowledgment before entering a codec call.
    [[nodiscard]] bool needs_advance() const noexcept;
    [[nodiscard]] Result<void> advance();
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] ExportImageMemory memory() const noexcept;
    [[nodiscard]] Result<EncodedImageOutput> take_output();
    [[nodiscard]] Result<void> discard() noexcept;

  private:
    ExportImageInput() noexcept;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace elf3d::gltf::exporter_detail
