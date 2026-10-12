#include <elf3d/internal/scene.h>

#include "tangent_space.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace elf3d::scene {
namespace {
constexpr std::uint64_t document_mesh_handle_marker = std::uint64_t{1} << 63U;
struct MaterialGroup {
    MaterialId material;
};

[[nodiscard]] std::vector<MaterialGroup> material_groups(const PrimitiveView& primitive)
{
    return {{primitive.material}};
}

[[nodiscard]] Bounds3 geometry_bounds(std::span<const Float3> positions)
{
    Bounds3 bounds{positions.front(), positions.front()};
    for (const Float3 position : positions) {
        bounds.minimum = {std::min(bounds.minimum.x, position.x),
                          std::min(bounds.minimum.y, position.y),
                          std::min(bounds.minimum.z, position.z)};
        bounds.maximum = {std::max(bounds.maximum.x, position.x),
                          std::max(bounds.maximum.y, position.y),
                          std::max(bounds.maximum.z, position.z)};
    }
    return bounds;
}

using TangentSources = std::array<std::optional<PrimitiveData>, 2>;

[[nodiscard]] Result<std::optional<std::uint32_t>> normal_texcoord(const Document& document,
                                                                   MaterialId id)
{
    const auto material = document.material(id);
    if (!material)
        return material.error();
    const auto& description = material.value().description;
    if (description.unlit)
        return std::optional<std::uint32_t>{};
    if (description.normal_texture.is_valid())
        return std::optional<std::uint32_t>{description.normal_texture_mapping.texcoord_set};
    if (description.clearcoat && description.clearcoat->factor > 0 &&
        description.clearcoat->normal_texture.is_valid())
        return std::optional<std::uint32_t>{
            description.clearcoat->normal_texture_mapping.texcoord_set};
    return std::optional<std::uint32_t>{};
}

[[nodiscard]] Result<bool> preserve_authored_tangents(const PrimitiveDataView& data,
                                                      const Document& document, MaterialId id)
{
    if (data.tangents.empty())
        return false;
    const auto material = document.material(id);
    if (!material)
        return material.error();
    // Authored glTF tangents belong to the base normal slot. A coat-only material
    // generates its selected raw-UV basis instead of assuming that basis matches.
    return material.value().description.normal_texture.is_valid();
}

[[nodiscard]] Result<TangentSources> prepare_tangent_sources(const PrimitiveDataView& data,
                                                             const Document& document,
                                                             std::span<const MaterialGroup> groups)
{
    TangentSources sources;
    std::array<bool, 2> attempted{};
    for (const auto& group : groups) {
        const auto mapping = normal_texcoord(document, group.material);
        if (!mapping)
            return mapping.error();
        if (!mapping.value() || attempted[*mapping.value()])
            continue;
        const auto preserve = preserve_authored_tangents(data, document, group.material);
        if (!preserve)
            return preserve.error();
        if (preserve.value())
            continue;
        const auto texcoord = *mapping.value();
        attempted[texcoord] = true;
        auto generated = tangent_detail::prepare_tangent_geometry(data, texcoord);
        if (!generated)
            return generated.error();
        sources[texcoord] = std::move(generated).value();
    }
    return sources;
}

struct PreparedGeometry {
    Bounds3 bounds;
    std::optional<PrimitiveData> data;
};

[[nodiscard]] Result<PreparedGeometry> prepare_group_geometry(const PrimitiveView& view,
                                                              const MaterialGroup& group,
                                                              const Document& document,
                                                              TangentSources& sources)
{
    const auto mapping = normal_texcoord(document, group.material);
    if (!mapping)
        return mapping.error();
    PrimitiveData* generated = nullptr;
    if (mapping.value() && sources[*mapping.value()])
        generated = &*sources[*mapping.value()];
    const auto data = generated != nullptr ? generated->view() : view.data;
    PreparedGeometry prepared{view.bounds};
    if (generated != nullptr) {
        prepared.data = std::move(*generated);
    }
    if (prepared.data)
        prepared.bounds = geometry_bounds(prepared.data->positions);
    return prepared;
}

} // namespace

Result<void> Storage::prepare_document_groups(const PrimitiveView& view,
                                              std::vector<std::size_t>& groups)
{
    if (groups.empty()) {
        const auto material_groups = scene::material_groups(view);
        if (material_groups.size() >
            std::numeric_limits<std::uint32_t>::max() - document_mesh_primitives_.size()) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "Runtime material groups exceed the mesh identity range"};
        }
        auto sources = prepare_tangent_sources(view.data, *document_, material_groups);
        if (!sources)
            return sources.error();
        std::vector<DocumentRuntimePrimitive> staged;
        for (const auto& group : material_groups) {
            auto geometry = prepare_group_geometry(view, group, *document_, sources.value());
            if (!geometry)
                return geometry.error();
            auto prepared = std::move(geometry).value();
            staged.push_back({view.id, group.material, prepared.bounds, std::move(prepared.data)});
        }
        for (auto& prepared : staged) {
            groups.push_back(document_mesh_primitives_.size());
            document_mesh_primitives_.push_back(std::move(prepared));
        }
    }
    return {};
}

Result<std::vector<MeshHandle>> Storage::document_mesh_handles(PrimitiveId primitive)
{
    if (document_ == nullptr) {
        return Error{ErrorCode::invalid_argument,
                     "A document mesh handle requires a scene document"};
    }

    const Result<PrimitiveView> view = document_->primitive(primitive);
    if (!view) {
        return view.error();
    }

    const std::uint64_t value = primitive.debug_value();
    if (value == 0 || value >= document_mesh_handle_marker ||
        value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - 1U)) {
        return Error{ErrorCode::invalid_mesh_handle,
                     "The document primitive cannot receive a runtime mesh handle"};
    }

    const std::size_t index = static_cast<std::size_t>(value);
    if (index >= document_primitive_groups_.size()) {
        document_primitive_groups_.resize(index + 1U);
    }
    auto& groups = document_primitive_groups_[index];
    const auto prepared = prepare_document_groups(view.value(), groups);
    if (!prepared)
        return prepared.error();
    std::vector<MeshHandle> result;
    result.reserve(groups.size());
    for (const std::size_t group : groups) {
        result.push_back(detail::SceneHandleAccess::create_mesh(
            id_, document_mesh_handle_marker | static_cast<std::uint64_t>(group)));
    }
    return result;
}

} // namespace elf3d::scene
