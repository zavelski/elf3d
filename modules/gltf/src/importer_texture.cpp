module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "importer_internal.hpp"

#include <cgltf.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.gltf;

import elf.core;
import elf.math;
import elf.model;

namespace elf3d::gltf::importer_detail {

[[nodiscard]] Result<std::vector<float>>
unpack_float3(const cgltf_accessor& accessor, ErrorCode error_code, std::string_view context) {
    if (accessor.count > std::numeric_limits<std::size_t>::max() / 3) {
        return Error{ErrorCode::size_overflow,
                     std::string{context} + " accessor size overflows addressable memory"};
    }
    if (accessor.type != cgltf_type_vec3 || accessor.count == 0) {
        return Error{error_code, std::string{context} + " requires a non-empty VEC3 accessor"};
    }
    try {
        const std::size_t float_count = static_cast<std::size_t>(accessor.count) * 3;
        std::vector<float> values(float_count);
        if (cgltf_accessor_unpack_floats(&accessor, values.data(), float_count) != float_count) {
            return Error{ErrorCode::invalid_accessor,
                         std::string{context} + " could not be decoded from its accessor"};
        }
        return values;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<std::vector<float>> unpack_float2(const cgltf_accessor& accessor,
                                                       std::string_view context) {
    if (accessor.count > std::numeric_limits<std::size_t>::max() / 2) {
        return Error{ErrorCode::size_overflow,
                     std::string{context} + " accessor size overflows addressable memory"};
    }
    if (accessor.type != cgltf_type_vec2 || accessor.count == 0) {
        return Error{ErrorCode::invalid_texcoord,
                     std::string{context} + " requires a non-empty VEC2 accessor"};
    }
    try {
        const std::size_t float_count = static_cast<std::size_t>(accessor.count) * 2;
        std::vector<float> values(float_count);
        if (cgltf_accessor_unpack_floats(&accessor, values.data(), float_count) != float_count) {
            return Error{ErrorCode::invalid_texcoord,
                         std::string{context} + " could not be decoded from its accessor"};
        }
        return values;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<std::vector<float>> unpack_color(const cgltf_accessor& accessor,
                                                      std::string_view context) {
    const std::size_t components = accessor.type == cgltf_type_vec3   ? 3U
                                   : accessor.type == cgltf_type_vec4 ? 4U
                                                                      : 0U;
    if (components == 0 || accessor.count == 0 ||
        accessor.count > std::numeric_limits<std::size_t>::max() / components) {
        return Error{ErrorCode::invalid_accessor,
                     std::string{context} + " requires a non-empty VEC3 or VEC4 accessor"};
    }
    try {
        const std::size_t float_count = static_cast<std::size_t>(accessor.count) * components;
        std::vector<float> values(float_count);
        if (cgltf_accessor_unpack_floats(&accessor, values.data(), float_count) != float_count) {
            return Error{ErrorCode::invalid_accessor,
                         std::string{context} + " could not be decoded from its accessor"};
        }
        return values;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] bool texture_transform_is_finite(const TextureTransform& transform) noexcept {
    return std::isfinite(transform.offset.x) && std::isfinite(transform.offset.y) &&
           std::isfinite(transform.scale.x) && std::isfinite(transform.scale.y) &&
           std::isfinite(transform.rotation_radians);
}

[[nodiscard]] Result<TextureMapping> texture_mapping(const cgltf_texture_view& view,
                                                     std::string_view context) {
    const cgltf_int selected_set =
        view.has_transform && view.transform.has_texcoord ? view.transform.texcoord : view.texcoord;
    if (selected_set < 0 ||
        selected_set >= static_cast<cgltf_int>(maximum_texture_coordinate_sets)) {
        return Error{ErrorCode::invalid_texcoord,
                     std::string{context} + " references unsupported TEXCOORD_" +
                         std::to_string(selected_set) + "; Elf3D supports UV sets 0 and 1"};
    }

    TextureMapping mapping;
    mapping.texcoord_set = static_cast<std::uint32_t>(selected_set);
    if (view.has_transform) {
        mapping.transform.offset = {view.transform.offset[0], view.transform.offset[1]};
        mapping.transform.scale = {view.transform.scale[0], view.transform.scale[1]};
        mapping.transform.rotation_radians = view.transform.rotation;
    }
    if (!texture_transform_is_finite(mapping.transform)) {
        return Error{ErrorCode::invalid_texcoord,
                     std::string{context} + " contains a non-finite texture transform"};
    }
    return mapping;
}

[[nodiscard]] TexcoordAvailability
primitive_texcoord_availability(const cgltf_primitive& primitive) {
    TexcoordAvailability availability{};
    for (cgltf_int set = 0; set < static_cast<cgltf_int>(maximum_texture_coordinate_sets); ++set) {
        availability[static_cast<std::size_t>(set)] =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, set) != nullptr;
    }
    return availability;
}

[[nodiscard]] Result<bool>
texture_view_uses_unavailable_texcoord(const cgltf_texture_view& view,
                                       const TexcoordAvailability& available_texcoords,
                                       std::string_view context) {
    if (view.texture == nullptr) {
        return false;
    }
    Result<TextureMapping> mapping = texture_mapping(view, context);
    if (!mapping) {
        return mapping.error();
    }
    return !available_texcoords[mapping.value().texcoord_set];
}

[[nodiscard]] Result<bool>
texture_slot_uses_unavailable_texcoord(const cgltf_texture_view& view, std::string_view slot,
                                       const TexcoordAvailability& available_texcoords,
                                       std::string_view context) {
    return texture_view_uses_unavailable_texcoord(view, available_texcoords,
                                                  std::string{context} + " " + std::string{slot});
}

[[nodiscard]] Result<bool>
metallic_roughness_uses_unavailable_texcoord(const cgltf_pbr_metallic_roughness& pbr,
                                             const TexcoordAvailability& available_texcoords,
                                             std::string_view context) {
    Result<bool> unavailable = texture_slot_uses_unavailable_texcoord(
        pbr.base_color_texture, "base-color texture", available_texcoords, context);
    if (!unavailable || unavailable.value()) {
        return unavailable;
    }
    return texture_slot_uses_unavailable_texcoord(
        pbr.metallic_roughness_texture, "metallic-roughness texture", available_texcoords, context);
}

[[nodiscard]] Result<bool>
common_material_uses_unavailable_texcoord(const cgltf_material& material,
                                          const TexcoordAvailability& available_texcoords,
                                          std::string_view context) {
    Result<bool> unavailable = texture_slot_uses_unavailable_texcoord(
        material.normal_texture, "normal texture", available_texcoords, context);
    if (!unavailable || unavailable.value()) {
        return unavailable;
    }
    unavailable = texture_slot_uses_unavailable_texcoord(
        material.occlusion_texture, "occlusion texture", available_texcoords, context);
    if (!unavailable || unavailable.value()) {
        return unavailable;
    }
    return texture_slot_uses_unavailable_texcoord(material.emissive_texture, "emissive texture",
                                                  available_texcoords, context);
}

[[nodiscard]] Result<bool>
material_uses_unavailable_texcoord(const cgltf_material& material,
                                   const TexcoordAvailability& available_texcoords,
                                   std::string_view context) {
    Result<bool> unavailable = false;
    if (material.has_pbr_metallic_roughness) {
        unavailable = metallic_roughness_uses_unavailable_texcoord(material.pbr_metallic_roughness,
                                                                   available_texcoords, context);
    } else if (material.has_pbr_specular_glossiness) {
        unavailable = texture_slot_uses_unavailable_texcoord(
            material.pbr_specular_glossiness.diffuse_texture, "specular-glossiness diffuse texture",
            available_texcoords, context);
    }
    if (!unavailable || unavailable.value()) {
        return unavailable;
    }
    return common_material_uses_unavailable_texcoord(material, available_texcoords, context);
}

[[nodiscard]] Result<ImportedTextureView> import_texture_view(ImportState& state,
                                                              PrimitiveTextureState primitive_state,
                                                              const cgltf_texture_view& view,
                                                              std::string_view slot_name,
                                                              std::string_view context) {
    Result<TextureMapping> mapping = texture_mapping(view, context);
    if (!mapping) {
        return mapping.error();
    }
    if (view.texture == nullptr) {
        return ImportedTextureView{{}, mapping.value()};
    }
    if (!primitive_state.available_texcoords[mapping.value().texcoord_set]) {
        primitive_state.primitive_specific_fallback = true;
        add_diagnostic(state.diagnostics, ModelLoadDiagnosticCategory::texture,
                       ModelLoadDiagnosticCode::texture_fallback,
                       std::string{slot_name} + " texture references TEXCOORD_" +
                           std::to_string(mapping.value().texcoord_set) +
                           " that this primitive does not provide; the slot was disabled",
                       std::string{context});
        return ImportedTextureView{{}, mapping.value()};
    }
    if (view.texture->image == nullptr && (view.texture->has_basisu || view.texture->has_webp)) {
        add_diagnostic(state.diagnostics, ModelLoadDiagnosticCategory::texture,
                       ModelLoadDiagnosticCode::texture_fallback,
                       std::string{slot_name} +
                           " texture uses an unsupported compressed/WebP source and has no "
                           "ordinary PNG/JPEG fallback; the slot was disabled",
                       std::string{context});
        return ImportedTextureView{{}, mapping.value()};
    }
    Result<TextureId> texture = texture_for(state, view.texture);
    if (!texture) {
        if (texture_failure_can_fallback(texture.error().code())) {
            add_diagnostic(state.diagnostics, ModelLoadDiagnosticCategory::texture,
                           ModelLoadDiagnosticCode::texture_fallback,
                           std::string{slot_name} +
                               " texture could not be imported and was "
                               "disabled: " +
                               texture.error().message(),
                           std::string{context});
            return ImportedTextureView{{}, mapping.value()};
        }
        return texture.error();
    }
    return ImportedTextureView{texture.value(), mapping.value()};
}

} // namespace elf3d::gltf::importer_detail
