#ifndef ELF3D_MODEL_DETAIL_DOCUMENT_WRITE_SOURCE_H
#define ELF3D_MODEL_DETAIL_DOCUMENT_WRITE_SOURCE_H

#include <elf3d/model.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

namespace elf3d::model::detail {

inline constexpr std::size_t write_source_chunk_bytes = 64 * 1024;
inline constexpr std::size_t write_source_chunk_elements = 4096;

enum class WriteSourceObject : std::uint8_t {
    root,
    asset,
    scene,
    node,
    camera,
    camera_projection,
    mesh,
    primitive,
    material,
    image,
    texture,
    sampler
};
inline constexpr std::size_t write_source_object_count = 12;

struct WriteSourceAddress {
    WriteSourceObject object = WriteSourceObject::root;
    std::size_t index = 0;
};

// Table positions are request coordinates under one captured owner/revision;
// canonical relationships in records and payloads retain typed Document IDs.
struct WriteSourceDescription {
    std::array<std::size_t, write_source_object_count> counts{};
    std::optional<DocumentSceneId> default_scene;
    std::size_t required_extensions = 0;
    bool preserved_metadata_stale = false;
};

struct WriteSourceRecord {
    WriteSourceAddress address;
    bool present = true;
    std::size_t name_bytes = 0;
    bool has_extras = false;
    std::size_t extras_bytes = 0;
    std::size_t extensions = 0;
    std::size_t links = 0;
    DocumentSceneId scene;
    NodeId node;
    MeshId mesh;
    PrimitiveId primitive;
    MaterialId material;
    ImageId image;
    TextureId texture;
    SamplerId sampler;
    std::optional<NodeId> parent;
    std::optional<MeshId> node_mesh;
    Float4x4 local_matrix;
    std::optional<PerspectiveCameraDescription> camera;
    std::optional<float> aspect_ratio;
    bool infinite_far_plane = false;
    std::optional<ModelCameraImageBasedLightingDescription> lighting;
    Bounds3 bounds;
    ModelMaterialDescription material_description;
    SamplerDescription sampler_description;
    ModelTextureDescription texture_description;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::rgba8_unorm;
    ModelImageMimeType mime = ModelImageMimeType::none;
    // Positions, normals, tangents, UV0, UV1, colors, indices; images use
    // the first two entries for pixels/source.
    std::array<std::size_t, 7> elements{};
};
static_assert(std::is_trivially_copyable_v<WriteSourceRecord>);
static_assert(sizeof(WriteSourceRecord) < 4096);

enum class WriteSourceText : std::uint8_t {
    name,
    extras,
    extension_name,
    extension_data,
    required_extension
};
struct WriteSourceTextRequest {
    WriteSourceAddress address;
    WriteSourceText text = WriteSourceText::name;
    std::size_t item = 0;
    std::size_t offset = 0;
};

enum class WriteSourceData : std::uint8_t {
    positions,
    normals,
    tangents,
    uv0,
    uv1,
    colors,
    indices,
    corners,
    children,
    roots,
    primitives,
    image_pixels,
    image_source,
    vertices
};
struct WriteSourceDataRequest {
    WriteSourceAddress address;
    WriteSourceData data = WriteSourceData::positions;
    std::size_t offset = 0;
};
struct WriteSourceChunk {
    std::size_t bytes = 0;
    std::size_t elements = 0;
    std::size_t total = 0;
};

// Bounded corner requests let file projection revisit the current Document
// without retaining its attribute streams or staging a second input model.
struct WriteSourceCorner {
    std::uint32_t vertex = 0;
    MaterialId material;
    Float3 position;
    Float3 normal;
    Float4 tangent;
    Float2 uv0;
    Float2 uv1;
    Color4 color;
};
static_assert(std::is_trivially_copyable_v<WriteSourceCorner>);

// Internal writer boundary, not an application API. Stores only identity,
// revision and small counts. Every source view expires within one call.
// Calls belong to the Document owner; this type creates no thread or channel.
class DocumentWriteSource final {
  public:
    [[nodiscard]] static Result<DocumentWriteSource> create(DocumentView source) noexcept;
    [[nodiscard]] bool matches(DocumentView source) const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] DocumentOwnerToken owner() const noexcept;
    // Restores an immutable SDK source capability, not a Document owner/view.
    [[nodiscard]] static DocumentWriteSource restore(DocumentOwnerToken owner,
                                                     std::uint64_t revision,
                                                     WriteSourceDescription description) noexcept;
    [[nodiscard]] WriteSourceDescription description() const noexcept;
    [[nodiscard]] Result<WriteSourceRecord> record(DocumentView source,
                                                   WriteSourceAddress address) const noexcept;
    [[nodiscard]] Result<WriteSourceChunk> read_text(DocumentView source,
                                                     WriteSourceTextRequest request,
                                                     std::span<char> destination) const noexcept;
    [[nodiscard]] Result<WriteSourceChunk>
    read_data(DocumentView source, WriteSourceDataRequest request,
              std::span<std::byte> destination) const noexcept;

  private:
    [[nodiscard]] bool valid_address(WriteSourceAddress address) const noexcept;
    DocumentOwnerToken owner_;
    std::uint64_t revision_ = 0;
    WriteSourceDescription description_;
};

} // namespace elf3d::model::detail

#endif
