#include "model_clone_test_support.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace elf3d;
using elf3d::model::tests::clone_triangle_data;
using elf3d::model::tests::make_clone_fixture;
using elf3d::model::tests::require_success;
using elf3d::model::tests::require_value;

template <typename Id> struct IdMap {
    std::vector<Id> source;
    std::vector<Id> copy;

    [[nodiscard]] Id mapped(Id id) const
    {
        if (!id.is_valid()) {
            return {};
        }
        for (std::size_t i = 0; i < source.size(); ++i) {
            if (source[i] == id) {
                return copy[i];
            }
        }
        fatal_error("Clone test cannot map a source ID by public enumeration");
    }
};

template <typename Getter, typename CopyGetter>
[[nodiscard]] auto id_map(std::size_t count, Getter source_at, CopyGetter copy_at)
{
    using Id = decltype(require_value(source_at(0)).id);
    IdMap<Id> result;
    for (std::size_t i = 0; i < count; ++i) {
        result.source.push_back(require_value(source_at(i)).id);
        result.copy.push_back(require_value(copy_at(i)).id);
    }
    return result;
}

template <typename Id, typename SourceLookup, typename CopyLookup>
[[nodiscard]] bool rejects_foreign(const IdMap<Id>& ids, SourceLookup source_lookup,
                                   CopyLookup copy_lookup, ErrorCode error)
{
    for (std::size_t i = 0; i < ids.source.size(); ++i) {
        const auto source_foreign = source_lookup(ids.copy[i]);
        const auto copy_foreign = copy_lookup(ids.source[i]);
        const std::array<bool, 5> checks{ids.source[i] != ids.copy[i], !source_foreign,
                                         !copy_foreign,
                                         !source_foreign && source_foreign.error().code() == error,
                                         !copy_foreign && copy_foreign.error().code() == error};
        if (!std::ranges::all_of(checks, [](bool value) { return value; })) {
            return false;
        }
    }
    return true;
}

template <typename T>
[[nodiscard]] bool independent_values(std::span<const T> a, std::span<const T> b)
{
    return std::ranges::equal(a, b) && (a.empty() || a.data() != b.data());
}

[[nodiscard]] bool independent_text(std::string_view a, std::string_view b)
{
    return a == b && (a.empty() || a.data() != b.data());
}

[[nodiscard]] bool same_metadata(ModelJsonMetadataView a, ModelJsonMetadataView b)
{
    if (a.extras_json.has_value() != b.extras_json.has_value()) {
        return false;
    }
    if (a.extras_json && !independent_text(*a.extras_json, *b.extras_json)) {
        return false;
    }
    if (a.extensions.size() != b.extensions.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.extensions.size(); ++i) {
        const auto& left = a.extensions[i];
        const auto& right = b.extensions[i];
        if (!independent_text(left.name, right.name) || !independent_text(left.data, right.data)) {
            return false;
        }
    }
    return true;
}

template <typename Id>
[[nodiscard]] bool same_references(std::span<const Id> a, std::span<const Id> b,
                                   const IdMap<Id>& ids)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ids.mapped(a[i]) != b[i]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool same_camera(const NodeView& a, const NodeView& b, const IdMap<ImageId>& images)
{
    const auto& left = a.camera_properties;
    const auto& right = b.camera_properties;
    auto lighting = left.image_based_lighting;
    if (lighting) {
        lighting->image = images.mapped(lighting->image);
    }
    const std::array<bool, 7> checks{
        a.perspective_camera == b.perspective_camera,
        independent_text(left.name, right.name),
        left.aspect_ratio == right.aspect_ratio,
        left.infinite_far_plane == right.infinite_far_plane,
        lighting == right.image_based_lighting,
        same_metadata(left.metadata, right.metadata),
        same_metadata(left.projection_metadata, right.projection_metadata)};
    return std::ranges::all_of(checks, [](bool value) { return value; });
}

[[nodiscard]] bool same_hierarchy(const Document& a, const Document& b)
{
    const auto scenes = id_map(
        a.scene_count(), [&](auto i) { return a.scene_at(i); },
        [&](auto i) { return b.scene_at(i); });
    const auto nodes = id_map(
        a.node_count(), [&](auto i) { return a.node_at(i); }, [&](auto i) { return b.node_at(i); });
    const auto meshes = id_map(
        a.mesh_count(), [&](auto i) { return a.mesh_at(i); }, [&](auto i) { return b.mesh_at(i); });
    const auto images = id_map(
        a.image_count(), [&](auto i) { return a.image_at(i); },
        [&](auto i) { return b.image_at(i); });
    const std::array<bool, 3> identities{
        scenes.mapped(a.default_scene().value_or(DocumentSceneId{})) ==
            b.default_scene().value_or(DocumentSceneId{}),
        rejects_foreign(
            scenes, [&](auto id) { return a.scene(id); }, [&](auto id) { return b.scene(id); },
            ErrorCode::invalid_document_scene_id),
        rejects_foreign(
            nodes, [&](auto id) { return a.node(id); }, [&](auto id) { return b.node(id); },
            ErrorCode::invalid_node_id)};
    if (!std::ranges::all_of(identities, [](bool value) { return value; })) {
        return false;
    }
    for (std::size_t i = 0; i < a.scene_count(); ++i) {
        const auto left = require_value(a.scene_at(i));
        const auto right = require_value(b.scene_at(i));
        const std::array<bool, 3> checks{independent_text(left.name, right.name),
                                         same_references(left.roots, right.roots, nodes),
                                         same_metadata(left.metadata, right.metadata)};
        if (!std::ranges::all_of(checks, [](bool value) { return value; }))
            return false;
    }
    for (std::size_t i = 0; i < a.node_count(); ++i) {
        const auto left = require_value(a.node_at(i));
        const auto right = require_value(b.node_at(i));
        const std::array<bool, 7> checks{
            independent_text(left.name, right.name),
            nodes.mapped(left.parent.value_or(NodeId{})) == right.parent.value_or(NodeId{}),
            meshes.mapped(left.mesh.value_or(MeshId{})) == right.mesh.value_or(MeshId{}),
            same_references(left.children, right.children, nodes),
            left.local_matrix == right.local_matrix,
            same_camera(left, right, images),
            same_metadata(left.metadata, right.metadata)};
        if (!std::ranges::all_of(checks, [](bool value) { return value; }))
            return false;
    }
    return true;
}

[[nodiscard]] bool same_geometry(PrimitiveDataView a, PrimitiveDataView b)
{
    const std::array<bool, 7> checks{
        independent_values(a.positions, b.positions), independent_values(a.normals, b.normals),
        independent_values(a.texcoord0, b.texcoord0), independent_values(a.texcoord1, b.texcoord1),
        independent_values(a.colors, b.colors),       independent_values(a.indices, b.indices),
        independent_values(a.tangents, b.tangents)};
    return std::ranges::all_of(checks, [](bool value) { return value; });
}

[[nodiscard]] bool same_meshes(const Document& a, const Document& b)
{
    const auto meshes = id_map(
        a.mesh_count(), [&](auto i) { return a.mesh_at(i); }, [&](auto i) { return b.mesh_at(i); });
    const auto primitives = id_map(
        a.primitive_count(), [&](auto i) { return a.primitive_at(i); },
        [&](auto i) { return b.primitive_at(i); });
    const auto materials = id_map(
        a.material_count(), [&](auto i) { return a.material_at(i); },
        [&](auto i) { return b.material_at(i); });
    const std::array<bool, 2> identities{
        rejects_foreign(
            meshes, [&](auto id) { return a.mesh(id); }, [&](auto id) { return b.mesh(id); },
            ErrorCode::invalid_mesh_id),
        rejects_foreign(
            primitives, [&](auto id) { return a.primitive(id); },
            [&](auto id) { return b.primitive(id); }, ErrorCode::invalid_primitive_id)};
    if (!std::ranges::all_of(identities, [](bool value) { return value; }))
        return false;
    for (std::size_t i = 0; i < a.mesh_count(); ++i) {
        const auto left = require_value(a.mesh_at(i));
        const auto right = require_value(b.mesh_at(i));
        const std::array<bool, 4> checks{
            independent_text(left.name, right.name), left.bounds == right.bounds,
            same_references(left.primitives, right.primitives, primitives),
            same_metadata(left.metadata, right.metadata)};
        if (!std::ranges::all_of(checks, [](bool value) { return value; }))
            return false;
    }
    for (std::size_t i = 0; i < a.primitive_count(); ++i) {
        const auto left = require_value(a.primitive_at(i));
        const auto right = require_value(b.primitive_at(i));
        const std::array<bool, 5> checks{
            meshes.mapped(left.mesh) == right.mesh,
            materials.mapped(left.material) == right.material, left.bounds == right.bounds,
            same_geometry(left.data, right.data), same_metadata(left.metadata, right.metadata)};
        if (!std::ranges::all_of(checks, [](bool value) { return value; }))
            return false;
    }
    return true;
}

[[nodiscard]] ModelMaterialDescription mapped_material(ModelMaterialDescription value,
                                                       const IdMap<TextureId>& textures)
{
    value.base_color_texture = textures.mapped(value.base_color_texture);
    value.metallic_roughness_texture = textures.mapped(value.metallic_roughness_texture);
    value.normal_texture = textures.mapped(value.normal_texture);
    value.occlusion_texture = textures.mapped(value.occlusion_texture);
    value.emissive_texture = textures.mapped(value.emissive_texture);
    value.specular_texture = textures.mapped(value.specular_texture);
    value.specular_color_texture = textures.mapped(value.specular_color_texture);
    if (value.volume)
        value.volume->thickness_texture = textures.mapped(value.volume->thickness_texture);
    if (value.transmission)
        value.transmission->texture = textures.mapped(value.transmission->texture);
    if (value.clearcoat) {
        value.clearcoat->texture = textures.mapped(value.clearcoat->texture);
        value.clearcoat->roughness_texture = textures.mapped(value.clearcoat->roughness_texture);
        value.clearcoat->normal_texture = textures.mapped(value.clearcoat->normal_texture);
    }
    return value;
}

[[nodiscard]] bool same_materials(const Document& a, const Document& b)
{
    const auto materials = id_map(
        a.material_count(), [&](auto i) { return a.material_at(i); },
        [&](auto i) { return b.material_at(i); });
    const auto textures = id_map(
        a.texture_count(), [&](auto i) { return a.texture_at(i); },
        [&](auto i) { return b.texture_at(i); });
    if (!rejects_foreign(
            materials, [&](auto id) { return a.material(id); },
            [&](auto id) { return b.material(id); }, ErrorCode::invalid_material_id))
        return false;
    for (std::size_t i = 0; i < a.material_count(); ++i) {
        const auto left = require_value(a.material_at(i));
        const auto right = require_value(b.material_at(i));
        const std::array<bool, 3> checks{
            mapped_material(left.description, textures) == right.description,
            independent_text(left.description.name, right.description.name),
            same_metadata(left.metadata, right.metadata)};
        if (!std::ranges::all_of(checks, [](bool value) { return value; }))
            return false;
    }
    return true;
}

enum class ImageStorage { independent, shared };

[[nodiscard]] bool same_images(const Document& a, const Document& b,
                               ImageStorage storage = ImageStorage::independent)
{
    const auto images = id_map(
        a.image_count(), [&](auto i) { return a.image_at(i); },
        [&](auto i) { return b.image_at(i); });
    if (!rejects_foreign(
            images, [&](auto id) { return a.image(id); }, [&](auto id) { return b.image(id); },
            ErrorCode::invalid_image_id))
        return false;
    for (std::size_t i = 0; i < a.image_count(); ++i) {
        const auto left = require_value(a.image_at(i));
        const auto right = require_value(b.image_at(i));
        const std::array<bool, 7> checks{
            left.width == right.width,
            left.height == right.height,
            left.format == right.format,
            left.source_mime_type == right.source_mime_type,
            storage == ImageStorage::shared ? left.pixels.data() == right.pixels.data()
                                            : independent_values(left.pixels, right.pixels),
            storage == ImageStorage::shared
                ? left.source_bytes.data() == right.source_bytes.data()
                : independent_values(left.source_bytes, right.source_bytes),
            same_metadata(left.metadata, right.metadata)};
        if (!std::ranges::all_of(checks, [](bool value) { return value; }))
            return false;
    }
    return true;
}

[[nodiscard]] bool same_texture_links(const Document& a, const Document& b)
{
    const auto textures = id_map(
        a.texture_count(), [&](auto i) { return a.texture_at(i); },
        [&](auto i) { return b.texture_at(i); });
    const auto images = id_map(
        a.image_count(), [&](auto i) { return a.image_at(i); },
        [&](auto i) { return b.image_at(i); });
    const auto samplers = id_map(
        a.sampler_count(), [&](auto i) { return a.sampler_at(i); },
        [&](auto i) { return b.sampler_at(i); });
    const std::array<bool, 2> identities{
        rejects_foreign(
            textures, [&](auto id) { return a.texture(id); },
            [&](auto id) { return b.texture(id); }, ErrorCode::invalid_texture_id),
        rejects_foreign(
            samplers, [&](auto id) { return a.sampler(id); },
            [&](auto id) { return b.sampler(id); }, ErrorCode::invalid_sampler_id)};
    if (!std::ranges::all_of(identities, [](bool value) { return value; }))
        return false;
    for (std::size_t i = 0; i < a.texture_count(); ++i) {
        const auto left = require_value(a.texture_at(i));
        const auto right = require_value(b.texture_at(i));
        const std::array<bool, 3> checks{
            images.mapped(left.description.image) == right.description.image,
            samplers.mapped(left.description.sampler) == right.description.sampler,
            same_metadata(left.metadata, right.metadata)};
        if (!std::ranges::all_of(checks, [](bool value) { return value; }))
            return false;
    }
    for (std::size_t i = 0; i < a.sampler_count(); ++i) {
        const auto left = require_value(a.sampler_at(i));
        const auto right = require_value(b.sampler_at(i));
        if (left.description != right.description || !same_metadata(left.metadata, right.metadata))
            return false;
    }
    return true;
}

[[nodiscard]] bool same_document(const Document& a, const Document& b,
                                 ImageStorage images = ImageStorage::independent)
{
    if (a.statistics() != b.statistics())
        return false;
    if (!std::ranges::equal(a.required_extensions(), b.required_extensions()))
        return false;
    for (std::size_t i = 0; i < a.required_extensions().size(); ++i) {
        if (!independent_text(a.required_extensions()[i], b.required_extensions()[i]))
            return false;
    }
    const std::array<bool, 11> checks{!validate_document(a.view()).has_errors(),
                                      !validate_document(b.view()).has_errors(),
                                      a.bounds() == b.bounds(),
                                      a.preserved_metadata_stale() == b.preserved_metadata_stale(),
                                      same_metadata(a.root_metadata(), b.root_metadata()),
                                      same_metadata(a.asset_metadata(), b.asset_metadata()),
                                      same_hierarchy(a, b),
                                      same_meshes(a, b),
                                      same_materials(a, b),
                                      same_images(a, b, images),
                                      same_texture_links(a, b)};
    return std::ranges::all_of(checks, [](bool value) { return value; });
}

[[nodiscard]] bool empty_and_moved_views()
{
    const auto missing = clone_document(DocumentView{});
    if (missing || missing.error().code() != ErrorCode::invalid_argument)
        return false;
    Document empty;
    auto copy = require_value(clone_document(empty.view()));
    if (!same_document(empty, copy))
        return false;
    const auto view = empty.view();
    Document moved = std::move(empty);
    const auto invalid = clone_document(view);
    if (invalid || invalid.error().code() != ErrorCode::invalid_argument)
        return false;
    return same_document(moved, require_value(clone_document(moved.view())));
}

[[nodiscard]] bool exact_copy_and_destruction()
{
    auto source = make_clone_fixture();
    auto copy = require_value(clone_document(source.view()));
    if (!same_document(source, copy))
        return false;
    auto twice = require_value(clone_document(copy.view()));
    if (!same_document(source, twice))
        return false;
    source = Document{};
    if (!same_document(make_clone_fixture(), copy))
        return false;
    copy = Document{};
    if (!same_document(make_clone_fixture(), twice))
        return false;
    auto disposable = require_value(clone_document(twice.view()));
    disposable = Document{};
    return same_document(make_clone_fixture(), twice);
}

void change_geometry_and_transform(Document& document)
{
    const auto node = require_value(document.node_at(2)).id;
    Float4x4 matrix;
    matrix.elements[12] = 99.0F;
    require_success(document.set_node_matrix(node, matrix));
    require_success(document.replace_primitive(require_value(document.primitive_at(0)).id,
                                               clone_triangle_data()));
}

[[nodiscard]] bool independent_mutation()
{
    auto source = make_clone_fixture();
    auto copy = require_value(clone_document(source.view()));
    change_geometry_and_transform(copy);
    if (!same_document(source, make_clone_fixture()))
        return false;
    copy = require_value(clone_document(source.view()));
    change_geometry_and_transform(source);
    return same_document(copy, make_clone_fixture());
}

[[nodiscard]] bool metadata_state_and_absent_default()
{
    auto source = make_clone_fixture();
    auto copy = require_value(clone_document(source.view()));
    Float4x4 matrix;
    matrix.elements[12] = 7.0F;
    require_success(copy.set_node_matrix(require_value(copy.node_at(0)).id, matrix));
    if (copy.preserved_metadata_stale())
        return false;
    const auto added = require_value(copy.create_node("Clone structural change"));
    if (!added.is_valid() || !copy.preserved_metadata_stale() || source.preserved_metadata_stale())
        return false;
    auto stale = require_value(clone_document(copy.view()));
    if (!same_document(copy, stale))
        return false;
    const auto report = validate_document(stale.view());
    const auto warning = std::ranges::find_if(report.diagnostics, [](const auto& diagnostic) {
        return diagnostic.code == DocumentDiagnosticCode::stale_preserved_metadata;
    });
    if (warning == report.diagnostics.end())
        return false;
    require_success(source.clear_default_scene());
    auto absent = require_value(clone_document(source.view()));
    return same_document(source, absent) && !absent.default_scene();
}

[[nodiscard]] bool explicit_geometry_metadata_policy()
{
    auto source = make_clone_fixture();
    auto candidate = require_value(clone_document(source.view()));
    const auto primitive = require_value(candidate.primitive_at(0));
    const auto extras = std::string{*primitive.metadata.extras_json};
    const auto geometry = clone_triangle_data();
    require_success(candidate.replace_primitive(
        primitive.id, geometry.view(), PrimitiveMetadataPolicy::preserve_verified_references));
    if (candidate.preserved_metadata_stale() || source.preserved_metadata_stale() ||
        require_value(candidate.primitive(primitive.id)).metadata.extras_json != extras ||
        require_value(candidate.primitive(primitive.id)).data.positions.data() ==
            geometry.positions.data()) {
        return false;
    }

    // Neither malformed replacement nor a foreign handle may change metadata
    // state or geometry. The explicit policy is not a validation bypass.
    auto malformed = clone_triangle_data();
    malformed.indices.front() = 0xffffffffU;
    const auto* positions = require_value(candidate.primitive(primitive.id)).data.positions.data();
    if (candidate.replace_primitive(primitive.id, std::move(malformed),
                                    PrimitiveMetadataPolicy::preserve_verified_references) ||
        candidate.replace_primitive(require_value(source.primitive_at(0)).id, clone_triangle_data(),
                                    PrimitiveMetadataPolicy::preserve_verified_references) ||
        candidate.preserved_metadata_stale() ||
        require_value(candidate.primitive(primitive.id)).data.positions.data() != positions) {
        return false;
    }

    require_success(candidate.replace_primitive(primitive.id, clone_triangle_data()));
    require_success(
        candidate.replace_primitive(primitive.id, clone_triangle_data(),
                                    PrimitiveMetadataPolicy::preserve_verified_references));
    return candidate.preserved_metadata_stale() && !source.preserved_metadata_stale();
}

[[nodiscard]] bool snapshot_lifetime()
{
    auto source = make_clone_fixture();
    auto first = require_value(snapshot_document(source.view()));
    auto second = require_value(snapshot_document(first.view()));
    if (!same_document(source, first, ImageStorage::shared) ||
        !same_document(first, second, ImageStorage::shared))
        return false;
    auto full = require_value(clone_document(first.view()));
    if (!same_document(source, full))
        return false;
    source = Document{};
    first = Document{};
    if (!same_document(make_clone_fixture(), second))
        return false;
    change_geometry_and_transform(second);
    if (!same_document(full, make_clone_fixture()))
        return false;
    const auto stale = require_value(snapshot_document(second.view()));
    return same_document(second, stale, ImageStorage::shared);
}

[[nodiscard]] bool snapshot_empty_and_invalid()
{
    if (snapshot_document(DocumentView{}))
        return false;
    Document empty;
    auto copy = require_value(snapshot_document(empty.view()));
    const auto view = empty.view();
    Document moved = std::move(empty);
    return !snapshot_document(view) && same_document(moved, copy);
}

[[nodiscard]] bool image_replacement_isolation()
{
    auto source = make_clone_fixture();
    auto snapshot = require_value(snapshot_document(source.view()));
    const auto old = require_value(source.image_at(0));
    const auto target = require_value(snapshot.image_at(0));
    std::vector<std::byte> pixels(old.pixels.begin(), old.pixels.end());
    pixels.front() ^= std::byte{0xff};
    const ModelImageDescription replacement{old.width, old.height, old.format, pixels};
    require_success(snapshot.replace_image(target.id, replacement));
    pixels.front() ^= std::byte{0xff};
    const auto changed = require_value(snapshot.image(target.id));
    if (changed.pixels.front() == old.pixels.front() || changed.pixels.data() == pixels.data() ||
        !changed.source_bytes.empty() || !snapshot.preserved_metadata_stale() ||
        source.preserved_metadata_stale() || !same_document(source, make_clone_fixture()))
        return false;
    auto previous = require_value(snapshot_document(source.view()));
    source = Document{};
    snapshot = std::move(previous);
    return same_document(snapshot, make_clone_fixture());
}

[[nodiscard]] bool image_replacement_failures()
{
    auto source = make_clone_fixture();
    auto snapshot = require_value(snapshot_document(source.view()));
    const auto target = require_value(snapshot.image_at(0));
    const ModelImageDescription self{target.width,  target.height,           target.format,
                                     target.pixels, target.source_mime_type, target.source_bytes};
    auto invalid = self;
    invalid.width = 0;
    if (snapshot.replace_image(target.id, invalid) ||
        snapshot.replace_image(require_value(source.image_at(0)).id, self) ||
        !same_document(source, snapshot, ImageStorage::shared))
        return false;
    constexpr std::array<std::byte, 8> exr{std::byte{0x76}, std::byte{0x2f}, std::byte{0x31},
                                           std::byte{1}, std::byte{2}};
    const ModelImageDescription encoded{
        0, 0, PixelFormat::rgba8_unorm, {}, ModelImageMimeType::exr, exr};
    if (snapshot.replace_image(target.id, encoded) ||
        !same_document(source, snapshot, ImageStorage::shared))
        return false;
    // The input aliases this owner; the source snapshot keeps the old view live.
    require_success(snapshot.replace_image(target.id, self));
    const auto copied = require_value(snapshot.image(target.id));
    return independent_values(copied.pixels, target.pixels) &&
           independent_values(copied.source_bytes, target.source_bytes) &&
           !validate_document(snapshot.view()).has_errors();
}

} // namespace

static_assert(noexcept(elf3d::clone_document(elf3d::DocumentView{})));

namespace {
bool same_reconstruction_metadata(DocumentView source, DocumentView target, MeshId mesh,
                                  NodeId node)
{
    return same_metadata(target.root_metadata(), source.root_metadata()) &&
           same_metadata(require_value(target.mesh(mesh)).metadata,
                         require_value(source.mesh_at(1)).metadata) &&
           same_metadata(require_value(target.node(node)).camera_properties.metadata,
                         require_value(source.node_at(4)).camera_properties.metadata);
}

bool reconstruction_metadata()
{
    auto source = make_clone_fixture();
    Document target;
    const auto scene = require_value(target.create_scene());
    const auto node = require_value(target.create_node());
    const auto mesh = require_value(target.create_mesh());
    const auto material = require_value(target.create_material());
    const auto primitive =
        require_value(target.create_primitive(mesh, material, clone_triangle_data()));
    const auto image = require_value(
        target.create_image_snapshot(source.view(), require_value(source.image_at(0)).id));
    const auto sampler = require_value(target.create_sampler());
    const auto texture = require_value(target.create_texture({image, sampler}));
    require_success(target.set_node_perspective_camera(
        node, *require_value(source.node_at(4)).perspective_camera));
    require_success(target.add_scene_root(scene, node));
    const std::array scenes{std::pair{require_value(source.scene_at(1)).id, scene}};
    const std::array nodes{std::pair{require_value(source.node_at(4)).id, node}};
    const std::array meshes{std::pair{require_value(source.mesh_at(1)).id, mesh}};
    const std::array primitives{std::pair{require_value(source.primitive_at(1)).id, primitive}};
    const std::array materials{std::pair{require_value(source.material_at(1)).id, material}};
    const std::array images{std::pair{require_value(source.image_at(0)).id, image}};
    const std::array textures{std::pair{require_value(source.texture_at(1)).id, texture}};
    const std::array samplers{std::pair{require_value(source.sampler_at(1)).id, sampler}};
    ModelMetadataMapping mapping{scenes,    nodes,  meshes,   primitives,
                                 materials, images, textures, samplers};
    const std::array duplicates{nodes[0], nodes[0]};
    mapping.nodes = duplicates;
    if (target.copy_verified_metadata(source.view(), mapping) || target.root_metadata().extras_json)
        return false;
    mapping.nodes = nodes;
    const auto input_image = require_value(source.image_at(0));
    if (require_value(target.image(image)).pixels.data() != input_image.pixels.data() ||
        target.image(input_image.id) || target.create_image_snapshot(target.view(), input_image.id))
        return false;
    require_success(target.copy_verified_metadata(source.view(), mapping));
    if (target.copy_verified_metadata(source.view(), mapping) ||
        !same_reconstruction_metadata(source.view(), target.view(), mesh, node))
        return false;
    source = Document{};
    return !validate_document(target.view()).has_errors() &&
           require_value(target.image(image)).pixels.data() == input_image.pixels.data();
}

bool reconstruction_metadata_refusals()
{
    auto source = make_clone_fixture();
    Document target;
    const auto node = require_value(target.create_node());
    const std::array mapping{std::pair{require_value(source.node_at(4)).id, node}};
    if (target.copy_verified_metadata(source.view(), {.nodes = mapping}) ||
        target.root_metadata().extras_json)
        return false;
    const auto added = require_value(source.create_node("Topology mutation"));
    if (!added.is_valid())
        return false;
    return !target.copy_verified_metadata(source.view(), {}) &&
           !target.copy_verified_metadata(DocumentView{}, {}) &&
           !target.root_metadata().extras_json;
}
} // namespace

int elf3d_model_clone_test()
{
    const std::array<bool, 11> checks{empty_and_moved_views(),
                                      exact_copy_and_destruction(),
                                      independent_mutation(),
                                      metadata_state_and_absent_default(),
                                      explicit_geometry_metadata_policy(),
                                      snapshot_lifetime(),
                                      snapshot_empty_and_invalid(),
                                      image_replacement_isolation(),
                                      image_replacement_failures(),
                                      reconstruction_metadata(),
                                      reconstruction_metadata_refusals()};
    for (std::size_t i = 0; i < checks.size(); ++i) {
        if (!checks[i]) {
            std::fprintf(stderr, "Document clone scenario %zu failed\n", i);
            return 1;
        }
    }
    return 0;
}
