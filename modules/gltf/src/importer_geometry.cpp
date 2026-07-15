module;

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>
#include <elf3d/core/result.h>

#include <cgltf.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.gltf;

import elf.core;

namespace elf3d::gltf::importer_geometry {
namespace {

[[noreturn]] void fatal_gltf_allocation_failure() noexcept {
    fatal_error("Elf3D glTF importer memory allocation failed");
}

[[noreturn]] void fatal_unexpected_gltf_boundary_exception() noexcept {
    fatal_error("Elf3D glTF importer encountered an unexpected exception");
}

[[nodiscard]] Result<std::vector<std::uint32_t>> sequential_indices(std::size_t vertex_count,
                                                                    std::string_view context) {
    if (vertex_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return Error{ErrorCode::invalid_accessor,
                     std::string{context} + " has too many non-indexed vertices"};
    }
    std::vector<std::uint32_t> indices(vertex_count);
    for (std::size_t index = 0; index < vertex_count; ++index) {
        indices[index] = static_cast<std::uint32_t>(index);
    }
    return indices;
}

[[nodiscard]] Result<std::vector<std::uint32_t>> source_indices(const cgltf_primitive& primitive,
                                                                std::size_t vertex_count,
                                                                std::string_view context) {
    if (primitive.indices == nullptr) {
        return sequential_indices(vertex_count, context);
    }
    const cgltf_accessor& accessor = *primitive.indices;
    if (accessor.type != cgltf_type_scalar) {
        return Error{ErrorCode::invalid_accessor,
                     std::string{context} + " uses a non-scalar index accessor"};
    }
    if (accessor.component_type != cgltf_component_type_r_8u &&
        accessor.component_type != cgltf_component_type_r_16u &&
        accessor.component_type != cgltf_component_type_r_32u) {
        return Error{ErrorCode::unsupported_index_type,
                     std::string{context} +
                         " uses an unsupported signed or non-integer index type"};
    }
    if (accessor.count == 0 || accessor.is_sparse) {
        return Error{ErrorCode::invalid_accessor,
                     std::string{context} + " requires a non-empty, non-sparse index accessor"};
    }
    std::vector<std::uint32_t> indices(accessor.count);
    if (cgltf_accessor_unpack_indices(&accessor, indices.data(), sizeof(std::uint32_t),
                                      accessor.count) != accessor.count) {
        return Error{ErrorCode::invalid_accessor,
                     std::string{context} + " index accessor could not be decoded"};
    }
    return indices;
}

[[nodiscard]] Result<void> validate_indices(std::span<const std::uint32_t> indices,
                                            std::size_t vertex_count, std::string_view context) {
    for (const std::uint32_t index : indices) {
        if (static_cast<std::size_t>(index) >= vertex_count) {
            return Error{ErrorCode::imported_index_out_of_range,
                         std::string{context} + " contains an index outside POSITION"};
        }
    }
    return {};
}

void append_expanded_triangle(std::vector<std::uint32_t>& triangles,
                              std::span<const std::uint32_t> indices, std::size_t index,
                              cgltf_primitive_type type) {
    if (type == cgltf_primitive_type_triangle_fan) {
        triangles.push_back(indices[0]);
        triangles.push_back(indices[index + 1]);
        triangles.push_back(indices[index + 2]);
    } else if (index % 2 == 0) {
        triangles.push_back(indices[index]);
        triangles.push_back(indices[index + 1]);
        triangles.push_back(indices[index + 2]);
    } else {
        triangles.push_back(indices[index + 1]);
        triangles.push_back(indices[index]);
        triangles.push_back(indices[index + 2]);
    }
}

[[nodiscard]] Result<std::vector<std::uint32_t>> expand_indices(cgltf_primitive_type type,
                                                                std::vector<std::uint32_t> indices,
                                                                std::string_view context) {
    if (type == cgltf_primitive_type_triangles) {
        if (indices.size() % 3 != 0) {
            return Error{ErrorCode::invalid_accessor,
                         std::string{context} +
                             " triangle-list index count is not divisible by three"};
        }
        return indices;
    }
    if (type != cgltf_primitive_type_triangle_strip && type != cgltf_primitive_type_triangle_fan) {
        return Error{ErrorCode::unsupported_primitive_mode,
                     std::string{context} + " uses an unsupported points or lines primitive mode"};
    }
    if (indices.size() < 3 || indices.size() - 2 > std::numeric_limits<std::size_t>::max() / 3) {
        return Error{ErrorCode::invalid_accessor,
                     std::string{context} + " triangle strip/fan requires at least three indices"};
    }
    std::vector<std::uint32_t> triangles;
    triangles.reserve((indices.size() - 2) * 3);
    for (std::size_t index = 0; index + 2 < indices.size(); ++index) {
        append_expanded_triangle(triangles, indices, index, type);
    }
    return triangles;
}

} // namespace

Result<std::vector<std::uint32_t>> import_indices(const cgltf_primitive& primitive,
                                                  std::size_t vertex_count,
                                                  std::string_view context) {
    try {
        Result<std::vector<std::uint32_t>> indices =
            source_indices(primitive, vertex_count, context);
        if (!indices) {
            return indices.error();
        }
        if (const Result<void> valid = validate_indices(indices.value(), vertex_count, context);
            !valid) {
            return valid.error();
        }
        return expand_indices(primitive.type, std::move(indices).value(), context);
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

} // namespace elf3d::gltf::importer_geometry
