#pragma once

#include "exporter_metadata_cursor.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace elf3d::gltf::exporter_detail {
struct ExportData;

struct OwnedExportScene {
    DocumentSceneId id;
    std::string name;
    std::vector<NodeId> roots;
    ModelJsonMetadata metadata;
};
struct OwnedExportNode {
    NodeId id;
    std::string name;
    std::optional<NodeId> parent;
    std::vector<NodeId> children;
    Float4x4 matrix;
    std::optional<MeshId> mesh;
    std::optional<PerspectiveCameraDescription> camera;
    ModelCameraPropertiesDescription camera_properties;
    ModelJsonMetadata metadata;
    ModelJsonMetadata camera_metadata;
    ModelJsonMetadata projection_metadata;
};
struct OwnedExportMesh {
    MeshId id;
    std::string name;
    std::vector<PrimitiveId> primitives;
    ModelJsonMetadata metadata;
};
struct OwnedExportPrimitive {
    PrimitiveId id;
    MeshId mesh;
    MaterialId material;
    Bounds3 bounds;
    std::array<std::size_t, 7> elements{};
    ModelJsonMetadata metadata;
};
struct OwnedExportMaterial {
    MaterialId id;
    std::string name;
    ModelMaterialDescription description;
    ModelJsonMetadata metadata;
};
struct OwnedExportImage {
    ImageId id;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::rgba8_unorm;
    ModelImageMimeType mime = ModelImageMimeType::none;
    std::array<std::size_t, 2> bytes{};
    ModelJsonMetadata metadata;
};
struct OwnedExportTexture {
    TextureId id;
    ModelTextureDescription description;
    ModelJsonMetadata metadata;
};
struct OwnedExportSampler {
    SamplerId id;
    SamplerDescription description;
    ModelJsonMetadata metadata;
};

// Worker-owned metadata/file membership only. Input methods consume copied
// records/bytes during the call; no source view or canonical payload is retained.
class ExportSourceMetadata final {
  public:
    [[nodiscard]] static Result<std::unique_ptr<ExportSourceMetadata>>
    create(model::detail::WriteSourceDescription description);
    [[nodiscard]] static Result<std::unique_ptr<ExportSourceMetadata>> capture(DocumentView source);
    [[nodiscard]] ExportMetadataRequest request() const noexcept;
    [[nodiscard]] Result<void> accept_record(const model::detail::WriteSourceRecord& record);
    [[nodiscard]] Result<void> accept_text(model::detail::WriteSourceChunk chunk,
                                           std::span<const char> bytes);
    [[nodiscard]] Result<void> accept_links(model::detail::WriteSourceChunk chunk,
                                            std::span<const std::byte> bytes);
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] Result<model::detail::WriteSourceRecord>
    primitive_source(std::size_t index) const noexcept;
    [[nodiscard]] Result<model::detail::WriteSourceRecord>
    image_source(std::size_t index) const noexcept;
    // Bound metadata/text/relationships borrow this owner. Geometry and image
    // payload spans stay empty; their receivers consume owned source packets.
    [[nodiscard]] Result<void> bind(ExportData& destination) const;

  private:
    using Phase = ExportMetadataCursor::Phase;
    [[nodiscard]] model::detail::WriteSourceAddress address() const noexcept;
    [[nodiscard]] ModelJsonMetadata& metadata();
    [[nodiscard]] ModelJsonMetadata& resource_metadata();
    [[nodiscard]] std::string& name();
    void store_record(const model::detail::WriteSourceRecord& record);
    void store_structural_record(const model::detail::WriteSourceRecord& record);
    void store_resource_record(const model::detail::WriteSourceRecord& record);
    [[nodiscard]] std::string& text();
    [[nodiscard]] Result<void> bind_structural(ExportData& destination) const;
    void bind_resources(ExportData& destination) const;
    ExportMetadataCursor cursor_;
    ModelJsonMetadata root_, asset_;
    std::vector<std::string> required_extensions_;
    std::vector<OwnedExportScene> scenes_;
    std::vector<OwnedExportNode> nodes_;
    std::vector<OwnedExportMesh> meshes_;
    std::vector<OwnedExportPrimitive> primitives_;
    std::vector<OwnedExportMaterial> materials_;
    std::vector<OwnedExportImage> images_;
    std::vector<OwnedExportTexture> textures_;
    std::vector<OwnedExportSampler> samplers_;
};
} // namespace elf3d::gltf::exporter_detail
