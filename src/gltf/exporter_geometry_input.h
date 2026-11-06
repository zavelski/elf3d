#pragma once

#include "exporter_index_output.h"

#include <model/document_write_source.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace elf3d::gltf::exporter_detail {
// Ordinary geometry preserves every authored vertex. It consumes bounded owned
// attribute packets directly into the output binary, without source views or
// geometry working files. Callers route geometry requiring vertex remapping
// to its projection.
class ExportGeometryInput final {
  public:
    [[nodiscard]] static Result<ExportGeometryInput> create(model::detail::WriteSourceRecord source,
                                                            const ExportData& metadata);
    [[nodiscard]] std::optional<model::detail::WriteSourceDataRequest> request() const noexcept;
    [[nodiscard]] Result<void> accept(model::detail::WriteSourceDataRequest request,
                                      model::detail::WriteSourceChunk chunk,
                                      std::span<const std::byte> bytes, ExportPackage& package);
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] Result<PrimitiveOutput> output() const noexcept;

  private:
    enum class Phase {
        positions,
        normals,
        tangents,
        uv0,
        uv1,
        colors,
        index_scan,
        indices,
        complete
    };
    [[nodiscard]] static Phase next(Phase phase) noexcept;
    [[nodiscard]] model::detail::WriteSourceData stream() const noexcept;
    [[nodiscard]] std::size_t count() const noexcept;
    [[nodiscard]] std::size_t stride() const noexcept;
    void advance() noexcept;
    [[nodiscard]] Result<void> consume(std::span<const std::byte> bytes, std::size_t elements,
                                       ExportPackage& package);
    [[nodiscard]] Result<void> consume_indices(std::span<const std::uint32_t> values,
                                               ExportPackage& package);
    [[nodiscard]] Result<void> consume_positions(std::span<const Float3> values,
                                                 ExportPackage& package);
    [[nodiscard]] Result<void> consume_normals(std::span<const Float3> values,
                                               ExportPackage& package);
    [[nodiscard]] Result<void> consume_uvs(std::span<const Float2> values, ExportPackage& package);
    [[nodiscard]] Result<void> consume_colors(std::span<const Color4> values,
                                              ExportPackage& package);
    [[nodiscard]] Result<void> append_range(Result<ByteRange> range);
    [[nodiscard]] Result<void> finish_stream(ExportPackage& package);
    [[nodiscard]] Result<void> finish_attribute(ExportPackage& package);
    [[nodiscard]] bool matches(model::detail::WriteSourceDataRequest request) const noexcept;
    [[nodiscard]] bool valid_chunk(model::detail::WriteSourceChunk chunk,
                                   std::size_t bytes) const noexcept;
    model::detail::WriteSourceRecord source_;
    Phase phase_ = Phase::positions;
    std::size_t offset_ = 0;
    std::optional<ByteRange> range_;
    std::optional<Bounds3> bounds_;
    std::uint32_t maximum_index_ = 0;
    ExportIndexOutput indices_;
    PrimitiveOutput output_;
};
// Synchronous driver only. The receiver above has no Document or Scene owner.
[[nodiscard]] Result<PrimitiveOutput>
drive_ordinary_geometry(std::size_t primitive, ExportPackage& package, DocumentView document);
} // namespace elf3d::gltf::exporter_detail
