#pragma once

#include <graphics/graphics.h>

namespace elf3d::graphics {
inline constexpr std::size_t maximum_mesh_upload_chunk = 256U * 1024U * 1024U;

struct MeshStorageDescription {
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    VertexLayout vertex_layout = VertexLayout::position_normal_float3;
};
enum class MeshPreparationPhase : std::uint8_t {
    objects,
    vertex_storage,
    index_storage,
    index_binding,
    attribute_pointer,
    attribute_enable,
    vertices,
    indices,
    validation,
    ready,
    failed,
    consumed
};
struct MeshUploadRequest {
    MeshPreparationPhase phase = MeshPreparationPhase::objects;
    std::size_t offset_bytes = 0;
    std::size_t bytes = 0;
    // Includes owning-thread/context validation and state capture/restoration.
    std::uint32_t driver_call_limit = 0;
};
struct MeshUploadStep {
    std::size_t uploaded_bytes = 0;
    std::uint32_t driver_calls = 0;
    bool ready = false;
    std::size_t allocated_bytes = 0;
};

// Owns incomplete backend resources and scalar cursors, never source data or a
// Scene/Document observer. Every method requires exclusive access on the owning
// graphics thread; advance also requires its current context. Request/transfer
// are CPU-only. Drawing/picking receives a mesh only after take_mesh succeeds.
class StaticMeshPreparation {
  public:
    virtual ~StaticMeshPreparation() noexcept = default;
    [[nodiscard]] virtual Result<MeshUploadRequest>
    request(std::size_t maximum_bytes = default_graphics_transfer_bytes) const noexcept = 0;
    // Payload is a synchronous borrow and must match the next request's kind,
    // remaining range and record alignment. A smaller complete-record chunk is
    // accepted; no call may upload more than maximum_mesh_upload_chunk bytes.
    [[nodiscard]] virtual Result<MeshUploadStep>
    advance(std::span<const std::byte> payload = {}) noexcept = 0;
    // Optional setup group; zero calls means the caller should use scalar steps.
    [[nodiscard]] virtual Result<MeshUploadStep> advance_setup(std::uint32_t) noexcept
    {
        return MeshUploadStep{};
    }
    [[nodiscard]] virtual std::uint64_t driver_calls() const noexcept = 0;
    [[nodiscard]] virtual Result<ResourceRetirementStep>
    advance_retirement(const ResourceRetirementBudget& budget) noexcept = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<StaticMesh>> take_mesh() noexcept = 0;
};
[[nodiscard]] Result<std::size_t> mesh_vertex_stride(VertexLayout layout) noexcept;
// Blocking consumers drive exactly the same preparation and bounded transfers.
[[nodiscard]] Result<std::unique_ptr<StaticMesh>>
complete_static_mesh_upload(Device& device, const StaticMeshDescription& description) noexcept;
} // namespace elf3d::graphics
