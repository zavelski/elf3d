#include "fixture.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

[[nodiscard]] bool same_hierarchy(const elf3d::Document& source, const elf3d::Document& result,
                                  elf3d::NodeView original, elf3d::NodeView node)
{
    if (original.parent.has_value() &&
        source.node(*original.parent).value().name != result.node(*node.parent).value().name) {
        return false;
    }
    for (std::size_t index = 0; index < original.children.size(); ++index) {
        if (source.node(original.children[index]).value().name !=
            result.node(node.children[index]).value().name) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool same_geometry(elf3d::PrimitiveDataView first, elf3d::PrimitiveDataView second)
{
    return std::equal(first.positions.begin(), first.positions.end(), second.positions.begin(),
                      second.positions.end()) &&
           std::equal(first.normals.begin(), first.normals.end(), second.normals.begin(),
                      second.normals.end()) &&
           std::equal(first.indices.begin(), first.indices.end(), second.indices.begin(),
                      second.indices.end());
}

[[nodiscard]] elf3d::Float4x4 expected_matrix(elf3d::NodeView node, bool zero)
{
    auto expected = node.local_matrix;
    if (!zero && !node.parent.has_value()) {
        // Fixture world center (5,16,15): a +90-degree Z turn adds (21,11,0).
        for (std::size_t column = 0; column < 4U; ++column) {
            expected.elements[column * 4U] = -node.local_matrix.elements[column * 4U + 1U];
            expected.elements[column * 4U + 1U] = node.local_matrix.elements[column * 4U];
        }
        expected.elements[12] += 21.0F;
        expected.elements[13] += 11.0F;
    }
    return expected;
}

[[nodiscard]] bool near_matrix(elf3d::Float4x4 first, elf3d::Float4x4 second)
{
    return std::equal(first.elements.begin(), first.elements.end(), second.elements.begin(),
                      [](float a, float b) { return std::abs(a - b) < 0.0001F; });
}

[[nodiscard]] int verify_nodes(const elf3d::Document& source, const elf3d::Document& result,
                               bool zero)
{
    for (std::size_t index = 0; index < source.node_count(); ++index) {
        const auto original = source.node_at(index).value();
        const auto node = result.node_at(index).value();
        if (node.name != original.name ||
            !near_matrix(node.local_matrix, expected_matrix(original, zero)) ||
            node.parent.has_value() != original.parent.has_value() ||
            node.children.size() != original.children.size() ||
            node.perspective_camera != original.perspective_camera) {
            return 3;
        }
        if (!same_hierarchy(source, result, original, node)) {
            return 3;
        }
    }
    return 0;
}

[[nodiscard]] int verify_scenes(const elf3d::Document& source, const elf3d::Document& result)
{
    for (std::size_t index = 0; index < source.scene_count(); ++index) {
        const auto original = source.scene_at(index).value();
        const auto scene = result.scene_at(index).value();
        if (scene.name != original.name || scene.roots.size() != original.roots.size()) {
            return 4;
        }
        for (std::size_t root = 0; root < original.roots.size(); ++root) {
            if (source.node(original.roots[root]).value().name !=
                result.node(scene.roots[root]).value().name) {
                return 4;
            }
        }
    }
    return 0;
}

[[nodiscard]] bool same_assets(const elf3d::Document& source, const elf3d::Document& result)
{
    auto first_material = source.material_at(0).value().description;
    auto second_material = result.material_at(0).value().description;
    // IDs are owner-scoped; compare the represented texture instead.
    first_material.base_color_texture = {};
    second_material.base_color_texture = {};
    const auto first = source.image_at(0).value();
    const auto second = result.image_at(0).value();
    return first_material == second_material && first.width == second.width &&
           first.height == second.height &&
           std::equal(first.pixels.begin(), first.pixels.end(), second.pixels.begin(),
                      second.pixels.end()) &&
           std::equal(first.source_bytes.begin(), first.source_bytes.end(),
                      second.source_bytes.begin(), second.source_bytes.end());
}

[[nodiscard]] int verify_document(const elf3d::Document& source, const elf3d::Document& result)
{
    if (source.statistics() != result.statistics() ||
        source.default_scene().has_value() != result.default_scene().has_value() ||
        elf3d::validate_document(result.view()).has_errors()) {
        return 2;
    }
    if (source.default_scene().has_value()) {
        const auto original = source.scene(*source.default_scene()).value();
        const auto selected = result.scene(*result.default_scene()).value();
        if (original.name != selected.name) {
            return 8;
        }
    }
    return 0;
}

[[nodiscard]] int verify_assets(const elf3d::Document& source, const elf3d::Document& result,
                                bool /*zero*/)
{
    if (!same_assets(source, result)) {
        return 7;
    }
    if (!same_geometry(source.primitive_at(0).value().data, result.primitive_at(0).value().data) ||
        result.node_at(1).value().mesh != result.node_at(2).value().mesh) {
        return 5;
    }
    if (source.root_metadata().extras_json.has_value() &&
        (!result.root_metadata().extras_json ||
         result.root_metadata().extras_json->find("\"cmd-test\"") == std::string_view::npos)) {
        return 6;
    }
    return 0;
}

} // namespace

int round_trip_test(std::string_view input, std::string_view output, bool zero)
{
    const auto before = elf3d::load_document(input);
    const auto after = elf3d::load_document(output);
    if (!before || !after) {
        return 1;
    }
    const auto& source = before.value().document;
    const auto& result = after.value().document;
    if (const int validated = verify_document(source, result); validated != 0) {
        return validated;
    }
    if (const int nodes = verify_nodes(source, result, zero); nodes != 0) {
        return nodes;
    }
    if (const int scenes = verify_scenes(source, result); scenes != 0) {
        return scenes;
    }
    return verify_assets(source, result, zero);
}
