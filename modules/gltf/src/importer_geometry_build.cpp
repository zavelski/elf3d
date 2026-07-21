module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "importer_internal.hpp"

#include <cgltf.h>

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

struct PrimitiveBuildState {
    ImportState& import_state;
    const cgltf_mesh& mesh;
    const cgltf_primitive& source;
    cgltf_size mesh_index;
    cgltf_size primitive_index;
    std::string context;
    PrimitiveData data;
    std::vector<std::uint32_t> indices;
    const cgltf_accessor* position_accessor = nullptr;
    std::size_t vertex_count = 0;
    TexcoordAvailability available_texcoords{};
};

enum class NormalImportOutcome : std::uint8_t {
    authored,
    missing,
    unusable,
};

[[nodiscard]] Result<void> import_positions(PrimitiveBuildState& state) {
    state.position_accessor = cgltf_find_accessor(&state.source, cgltf_attribute_type_position, 0);
    if (state.position_accessor == nullptr) {
        return Error{ErrorCode::missing_position_accessor,
                     state.context + " does not contain POSITION"};
    }
    Result<std::vector<float>> positions =
        unpack_float3(*state.position_accessor, ErrorCode::missing_position_accessor,
                      state.context + " POSITION");
    if (!positions) {
        return positions.error();
    }
    state.vertex_count = positions.value().size() / 3;
    state.data.positions.resize(state.vertex_count);
    for (std::size_t index = 0; index < state.vertex_count; ++index) {
        const Float3 position{positions.value()[index * 3], positions.value()[index * 3 + 1],
                              positions.value()[index * 3 + 2]};
        if (!math::is_finite(position)) {
            return Error{ErrorCode::non_finite_position,
                         state.context + " contains a non-finite POSITION value"};
        }
        state.data.positions[index] = position;
    }
    return {};
}

[[nodiscard]] Result<void> import_primitive_indices(PrimitiveBuildState& state) {
    Result<std::vector<std::uint32_t>> indices =
        import_indices(state.source, state.vertex_count, state.context);
    if (!indices) {
        return indices.error();
    }
    state.indices = std::move(indices).value();
    state.available_texcoords = primitive_texcoord_availability(state.source);
    return {};
}

[[nodiscard]] Result<void> import_texcoords(PrimitiveBuildState& state) {
    for (cgltf_int set = 0; set < static_cast<cgltf_int>(maximum_texture_coordinate_sets); ++set) {
        const cgltf_accessor* accessor =
            cgltf_find_accessor(&state.source, cgltf_attribute_type_texcoord, set);
        if (accessor == nullptr) {
            continue;
        }
        const std::string semantic = "TEXCOORD_" + std::to_string(set);
        if (accessor->count != state.position_accessor->count) {
            return Error{ErrorCode::mismatched_texcoord_count,
                         state.context + " " + semantic + " count does not match POSITION"};
        }
        Result<std::vector<float>> texcoords =
            unpack_float2(*accessor, state.context + " " + semantic);
        if (!texcoords) {
            return texcoords.error();
        }
        std::vector<Float2>& target = set == 0 ? state.data.texcoord0 : state.data.texcoord1;
        target.resize(state.vertex_count);
        for (std::size_t index = 0; index < state.vertex_count; ++index) {
            const Float2 texcoord{texcoords.value()[index * 2], texcoords.value()[index * 2 + 1]};
            if (!std::isfinite(texcoord.x) || !std::isfinite(texcoord.y)) {
                return Error{ErrorCode::invalid_texcoord,
                             state.context + " contains a non-finite " + semantic + " value"};
            }
            target[index] = texcoord;
        }
    }
    return {};
}

void diagnose_additional_texcoords(PrimitiveBuildState& state) {
    for (cgltf_size index = 0; index < state.source.attributes_count; ++index) {
        const cgltf_attribute& attribute = state.source.attributes[index];
        if (attribute.type == cgltf_attribute_type_texcoord &&
            attribute.index >= static_cast<cgltf_int>(maximum_texture_coordinate_sets)) {
            add_diagnostic(state.import_state.diagnostics, ModelLoadDiagnosticCategory::geometry,
                           ModelLoadDiagnosticCode::material_fallback,
                           "Additional UV sets beyond TEXCOORD_1 are not preserved unless "
                           "referenced, and referenced sets beyond 1 are rejected",
                           state.context);
            return;
        }
    }
}

[[nodiscard]] bool color_is_finite(const Color4& color) noexcept {
    return std::isfinite(color.red) && std::isfinite(color.green) && std::isfinite(color.blue) &&
           std::isfinite(color.alpha);
}

[[nodiscard]] Result<void> import_colors(PrimitiveBuildState& state) {
    const cgltf_accessor* accessor =
        cgltf_find_accessor(&state.source, cgltf_attribute_type_color, 0);
    if (accessor == nullptr) {
        return {};
    }
    if (accessor->count != state.position_accessor->count) {
        return Error{ErrorCode::invalid_accessor,
                     state.context + " COLOR_0 count does not match POSITION"};
    }
    Result<std::vector<float>> colors = unpack_color(*accessor, state.context + " COLOR_0");
    if (!colors) {
        return colors.error();
    }
    const std::size_t components = accessor->type == cgltf_type_vec3 ? 3U : 4U;
    state.data.colors.resize(state.vertex_count);
    for (std::size_t index = 0; index < state.vertex_count; ++index) {
        const Color4 color{colors.value()[index * components],
                           colors.value()[index * components + 1],
                           colors.value()[index * components + 2],
                           components == 4U ? colors.value()[index * components + 3] : 1.0F};
        if (!color_is_finite(color)) {
            return Error{ErrorCode::invalid_accessor,
                         state.context + " contains a non-finite COLOR_0 value"};
        }
        state.data.colors[index] = color;
    }
    return {};
}

[[nodiscard]] Result<NormalImportOutcome> import_authored_normals(PrimitiveBuildState& state) {
    const cgltf_accessor* accessor =
        cgltf_find_accessor(&state.source, cgltf_attribute_type_normal, 0);
    if (accessor == nullptr) {
        return NormalImportOutcome::missing;
    }
    if (accessor->count != state.position_accessor->count) {
        return Error{ErrorCode::mismatched_normal_count,
                     state.context + " NORMAL count does not match POSITION"};
    }
    Result<std::vector<float>> normals =
        unpack_float3(*accessor, ErrorCode::invalid_accessor, state.context + " NORMAL");
    if (!normals) {
        return normals.error();
    }
    state.data.normals.resize(state.vertex_count);
    for (std::size_t index = 0; index < state.vertex_count; ++index) {
        const Float3 normal{normals.value()[index * 3], normals.value()[index * 3 + 1],
                            normals.value()[index * 3 + 2]};
        const float length_squared =
            normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
        if (!math::is_finite(normal) || !std::isfinite(length_squared) ||
            length_squared <= 0.000000000001F) {
            if (!state.import_state.options.generate_missing_normals) {
                return Error{ErrorCode::invalid_accessor,
                             state.context + " contains an unusable NORMAL value"};
            }
            return NormalImportOutcome::unusable;
        }
        const float inverse_length = 1.0F / std::sqrt(length_squared);
        state.data.normals[index] =
            Float3{normal.x * inverse_length, normal.y * inverse_length, normal.z * inverse_length};
    }
    return NormalImportOutcome::authored;
}

[[nodiscard]] Result<void> generate_normal_fallback(PrimitiveBuildState& state,
                                                    NormalImportOutcome outcome) {
    if (outcome == NormalImportOutcome::authored) {
        return {};
    }
    if (!state.import_state.options.generate_missing_normals) {
        return Error{ErrorCode::missing_normals,
                     state.context + " has no NORMAL accessor and generation is disabled"};
    }
    std::uint64_t degenerate_count = 0;
    std::uint64_t fallback_count = 0;
    generate_normals(state.data.positions, state.data.normals, state.indices, degenerate_count,
                     fallback_count);
    const std::string prefix = outcome == NormalImportOutcome::unusable
                                   ? "Ignored unusable authored normals and generated replacements"
                                   : "Generated missing vertex normals";
    if (degenerate_count != 0 || fallback_count != 0) {
        add_diagnostic(state.import_state.diagnostics, ModelLoadDiagnosticCategory::geometry,
                       ModelLoadDiagnosticCode::degenerate_geometry,
                       prefix + "; ignored " + std::to_string(degenerate_count) +
                           " degenerate triangles and used +Y fallback normals for " +
                           std::to_string(fallback_count) + " vertices",
                       state.context);
    } else {
        add_diagnostic(state.import_state.diagnostics, ModelLoadDiagnosticCategory::geometry,
                       ModelLoadDiagnosticCode::generated_normals, prefix, state.context);
    }
    return {};
}

[[nodiscard]] Result<void> import_normals(PrimitiveBuildState& state) {
    Result<NormalImportOutcome> outcome = import_authored_normals(state);
    if (!outcome) {
        return outcome.error();
    }
    return generate_normal_fallback(state, outcome.value());
}

[[nodiscard]] Result<void> store_primitive(PrimitiveBuildState& state, ImportedMesh& result) {
    state.data.indices = std::move(state.indices);
    if (!result.mesh.has_value()) {
        const Result<MeshId> created = state.import_state.builder.create_mesh(
            state.mesh.name != nullptr ? std::string_view{state.mesh.name} : std::string_view{});
        if (!created) {
            return created.error();
        }
        result.mesh = created.value();
    }
    const Result<MaterialId> material =
        material_for(state.import_state, state.source.material, state.available_texcoords);
    if (!material) {
        return material.error();
    }
    const Result<PrimitiveId> primitive = state.import_state.builder.create_primitive(
        *result.mesh, material.value(), std::move(state.data));
    if (!primitive) {
        return primitive.error();
    }
    result.primitives[state.primitive_index] = primitive.value();
    ++result.primitive_count;
    return {};
}

[[nodiscard]] Result<void> import_primitive(ImportState& import_state, const cgltf_mesh& mesh,
                                            cgltf_size mesh_index, cgltf_size primitive_index,
                                            ImportedMesh& result) {
    const cgltf_primitive& source = mesh.primitives[primitive_index];
    const std::string context = mesh_context(mesh, mesh_index, primitive_index);
    if (!supported_primitive_type(source.type)) {
        add_diagnostic(import_state.diagnostics, ModelLoadDiagnosticCategory::geometry,
                       ModelLoadDiagnosticCode::skipped_unsupported_primitive,
                       context + " uses unsupported points or lines geometry and was skipped",
                       context);
        return {};
    }
    PrimitiveBuildState state{import_state, mesh, source, mesh_index, primitive_index, context};
    if (const Result<void> positions = import_positions(state); !positions) {
        return positions.error();
    }
    if (const Result<void> indices = import_primitive_indices(state); !indices) {
        return indices.error();
    }
    if (const Result<void> texcoords = import_texcoords(state); !texcoords) {
        return texcoords.error();
    }
    diagnose_additional_texcoords(state);
    if (const Result<void> colors = import_colors(state); !colors) {
        return colors.error();
    }
    if (const Result<void> normals = import_normals(state); !normals) {
        return normals.error();
    }
    return store_primitive(state, result);
}

[[nodiscard]] Result<ImportedMesh> import_mesh(ImportState& state, const cgltf_mesh& mesh,
                                               cgltf_size mesh_index) {
    try {
        ImportedMesh result;
        result.primitives.resize(mesh.primitives_count);
        for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
             ++primitive_index) {
            const Result<void> imported =
                import_primitive(state, mesh, mesh_index, primitive_index, result);
            if (!imported) {
                return imported.error();
            }
        }
        return result;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

} // namespace elf3d::gltf::importer_detail
