module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "importer_internal.hpp"

#include <mikktspace.h>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

module elf.gltf;

import elf.core;
import elf.model;

namespace elf3d::gltf::importer_detail {
namespace {

struct TangentGenerationState final {
    PrimitiveData& data;
    std::span<const Float2> texcoords;
    std::vector<Float4> corner_tangents;
};

[[nodiscard]] TangentGenerationState& generation_state(const SMikkTSpaceContext* context) {
    return *static_cast<TangentGenerationState*>(context->m_pUserData);
}

[[nodiscard]] std::size_t corner_index(int face, int vertex) noexcept {
    return static_cast<std::size_t>(face) * 3U + static_cast<std::size_t>(vertex);
}

[[nodiscard]] std::uint32_t source_index(const TangentGenerationState& state, int face,
                                         int vertex) noexcept {
    return state.data.indices[corner_index(face, vertex)];
}

[[nodiscard]] int face_count(const SMikkTSpaceContext* context) {
    return static_cast<int>(generation_state(context).data.indices.size() / 3U);
}

[[nodiscard]] int vertices_per_face(const SMikkTSpaceContext*, int) {
    return 3;
}

void read_position(const SMikkTSpaceContext* context, float output[], int face, int vertex) {
    const TangentGenerationState& state = generation_state(context);
    const Float3 value = state.data.positions[source_index(state, face, vertex)];
    output[0] = value.x;
    output[1] = value.y;
    output[2] = value.z;
}

void read_normal(const SMikkTSpaceContext* context, float output[], int face, int vertex) {
    const TangentGenerationState& state = generation_state(context);
    const Float3 value = state.data.normals[source_index(state, face, vertex)];
    output[0] = value.x;
    output[1] = value.y;
    output[2] = value.z;
}

void read_texcoord(const SMikkTSpaceContext* context, float output[], int face, int vertex) {
    const TangentGenerationState& state = generation_state(context);
    const Float2 value = state.texcoords[source_index(state, face, vertex)];
    output[0] = value.x;
    output[1] = value.y;
}

void write_tangent(const SMikkTSpaceContext* context, const float tangent[], float sign, int face,
                   int vertex) {
    TangentGenerationState& state = generation_state(context);
    state.corner_tangents[corner_index(face, vertex)] =
        Float4{tangent[0], tangent[1], tangent[2], sign < 0.0F ? -1.0F : 1.0F};
}

[[nodiscard]] float canonical_component(float value) noexcept {
    return value == 0.0F ? 0.0F : value;
}

[[nodiscard]] bool normalize_tangent(Float4& tangent) noexcept {
    const float length_squared =
        tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z;
    if (!std::isfinite(length_squared) || length_squared <= 0.000000000001F ||
        !std::isfinite(tangent.x) || !std::isfinite(tangent.y) || !std::isfinite(tangent.z)) {
        return false;
    }
    const float inverse_length = 1.0F / std::sqrt(length_squared);
    tangent.x = canonical_component(tangent.x * inverse_length);
    tangent.y = canonical_component(tangent.y * inverse_length);
    tangent.z = canonical_component(tangent.z * inverse_length);
    tangent.w = tangent.w < 0.0F ? -1.0F : 1.0F;
    return true;
}

struct SplitKey final {
    std::uint32_t source = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;
    std::uint32_t w = 0;

    bool operator==(const SplitKey&) const = default;
};

struct SplitKeyHash final {
    [[nodiscard]] std::size_t operator()(const SplitKey& key) const noexcept {
        std::size_t value = key.source;
        value = value * 16777619U ^ key.x;
        value = value * 16777619U ^ key.y;
        value = value * 16777619U ^ key.z;
        return value * 16777619U ^ key.w;
    }
};

[[nodiscard]] SplitKey split_key(std::uint32_t source, const Float4& tangent) noexcept {
    return SplitKey{
        source, std::bit_cast<std::uint32_t>(tangent.x), std::bit_cast<std::uint32_t>(tangent.y),
        std::bit_cast<std::uint32_t>(tangent.z), std::bit_cast<std::uint32_t>(tangent.w)};
}

template <typename Value>
void append_optional_vertex(std::vector<Value>& target, const std::vector<Value>& source,
                            std::uint32_t source_index_value) {
    if (!source.empty()) {
        target.push_back(source[source_index_value]);
    }
}

void reserve_vertex_attributes(PrimitiveData& data, std::size_t count) {
    data.positions.reserve(count);
    data.normals.reserve(count);
    data.texcoord0.reserve(count);
    data.texcoord1.reserve(count);
    data.colors.reserve(count);
    data.tangents.reserve(count);
    data.indices.reserve(count);
}

[[nodiscard]] Result<bool> split_tangent_vertices(PrimitiveData& data,
                                                  std::vector<Float4> corner_tangents) {
    PrimitiveData result;
    reserve_vertex_attributes(result, data.indices.size());
    std::unordered_map<SplitKey, std::uint32_t, SplitKeyHash> vertices;
    vertices.reserve(data.indices.size());

    for (std::size_t corner = 0; corner < data.indices.size(); ++corner) {
        Float4 tangent = corner_tangents[corner];
        if (!normalize_tangent(tangent)) {
            return false;
        }
        const std::uint32_t source = data.indices[corner];
        const SplitKey key = split_key(source, tangent);
        const auto existing = vertices.find(key);
        if (existing != vertices.end()) {
            result.indices.push_back(existing->second);
            continue;
        }
        if (result.positions.size() >= maximum_imported_vertices ||
            result.positions.size() >=
                static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "MikkTSpace vertex splitting exceeds the imported vertex limit"};
        }
        const std::uint32_t destination = static_cast<std::uint32_t>(result.positions.size());
        result.positions.push_back(data.positions[source]);
        append_optional_vertex(result.normals, data.normals, source);
        append_optional_vertex(result.texcoord0, data.texcoord0, source);
        append_optional_vertex(result.texcoord1, data.texcoord1, source);
        append_optional_vertex(result.colors, data.colors, source);
        result.tangents.push_back(tangent);
        result.indices.push_back(destination);
        vertices.emplace(key, destination);
    }
    data = std::move(result);
    return true;
}

} // namespace

Result<bool> generate_mikktspace_tangents(PrimitiveData& data, std::uint32_t texcoord_set) {
    if (data.indices.empty() || data.indices.size() % 3U != 0U || data.normals.empty() ||
        data.indices.size() / 3U > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    const std::span<const Float2> texcoords = texcoord_set == 1U
                                                  ? std::span<const Float2>{data.texcoord1}
                                                  : std::span<const Float2>{data.texcoord0};
    if (texcoords.size() != data.positions.size()) {
        return false;
    }

    TangentGenerationState state{data, texcoords, std::vector<Float4>(data.indices.size())};
    SMikkTSpaceInterface interface{};
    interface.m_getNumFaces = face_count;
    interface.m_getNumVerticesOfFace = vertices_per_face;
    interface.m_getPosition = read_position;
    interface.m_getNormal = read_normal;
    interface.m_getTexCoord = read_texcoord;
    interface.m_setTSpaceBasic = write_tangent;
    SMikkTSpaceContext context{&interface, &state};
    if (genTangSpaceDefault(&context) == 0) {
        return false;
    }
    return split_tangent_vertices(data, std::move(state.corner_tangents));
}

} // namespace elf3d::gltf::importer_detail
