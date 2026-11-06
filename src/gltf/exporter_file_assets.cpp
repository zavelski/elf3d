#include "exporter_file_validation.h"

#include <model/document_storage.h>

#include <array>

namespace elf3d::gltf::exporter_detail {
using namespace model::detail;
namespace {
[[nodiscard]] bool valid_material_textures(const ModelMaterialDescription& value,
                                           const ExportData& document)
{
    const std::array<TextureId, material_texture_slot_count> slots{
        value.base_color_texture,
        value.metallic_roughness_texture,
        value.normal_texture,
        value.occlusion_texture,
        value.emissive_texture,
        value.specular_texture,
        value.specular_color_texture,
        value.transmission ? value.transmission->texture : TextureId{},
        value.volume ? value.volume->thickness_texture : TextureId{},
        value.clearcoat ? value.clearcoat->texture : TextureId{},
        value.clearcoat ? value.clearcoat->roughness_texture : TextureId{},
        value.clearcoat ? value.clearcoat->normal_texture : TextureId{}};
    for (const auto id : slots) {
        if (id.is_valid() && !export_table_index(document.textures, id))
            return false;
    }
    return true;
}
[[nodiscard]] Result<void> validate_materials(const ExportData& document)
{
    for (const auto& material : document.materials) {
        if (!export_table_index(document.materials, material.id) ||
            !valid_material_textures(material.description, document))
            return Error{ErrorCode::invalid_texture_id,
                         "Exported material has invalid identity or texture links"};
        if (!valid_material_factors(material.description) ||
            !valid_material_mappings(material.description))
            return Error{ErrorCode::invalid_argument,
                         "Exported material contains invalid factors or mappings"};
    }
    return {};
}
[[nodiscard]] Result<void> validate_samplers(const ExportData& document)
{
    for (const auto& sampler : document.samplers) {
        const auto& value = sampler.description;
        if (!export_table_index(document.samplers, sampler.id) || !valid_wrap(value.wrap_u) ||
            !valid_wrap(value.wrap_v) || !valid_filter(value.min_filter) ||
            !valid_mag_filter(value.mag_filter))
            return Error{ErrorCode::invalid_sampler_id,
                         "Exported sampler contains invalid identity, wrap or filter"};
    }
    return {};
}
} // namespace
Result<void> validate_export_assets(const ExportData& document)
{
    if (const auto materials = validate_materials(document); !materials)
        return materials.error();
    for (const auto& texture : document.textures) {
        if (!export_table_index(document.textures, texture.id) ||
            !export_table_index(document.images, texture.description.image) ||
            !export_table_index(document.samplers, texture.description.sampler))
            return Error{ErrorCode::invalid_texture_id,
                         "Exported texture references an invalid image or sampler"};
    }
    for (const auto& image : document.images) {
        if (!export_table_index(document.images, image.id))
            return Error{ErrorCode::invalid_image_id, "Exported image has invalid identity"};
    }
    return validate_samplers(document);
}
} // namespace elf3d::gltf::exporter_detail
