#include <elf3d/model.h>

#include <cstdint>
#include <utility>

import elf.model;

namespace {

[[nodiscard]] elf3d::PrimitiveData make_quad() {
    elf3d::PrimitiveData data;
    data.positions = {
        {-1.0F, -1.0F, 0.0F}, {1.0F, -1.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, {-1.0F, 1.0F, 0.0F}};
    data.normals.assign(data.positions.size(), elf3d::Float3{0.0F, 0.0F, 1.0F});
    data.indices = {0U, 1U, 2U, 0U, 2U, 3U};
    return data;
}

[[nodiscard]] elf3d::PrimitiveData make_reduced_triangle() {
    elf3d::PrimitiveData data;
    data.positions = {{-1.0F, -1.0F, 0.0F}, {1.0F, -1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    data.normals.assign(data.positions.size(), elf3d::Float3{0.0F, 0.0F, 1.0F});
    data.indices = {0U, 1U, 2U};
    return data;
}

} // namespace

int elf3d_model_processing_test() {
    elf3d::Document document;
    const auto scene = document.create_scene("Processing");
    const auto node = document.create_node("Reduced mesh");
    const auto mesh = document.create_mesh("Source quad");
    const auto material = document.create_material();
    if (!scene || !node || !mesh || !material) {
        return 1;
    }

    const auto primitive = document.create_primitive(mesh.value(), material.value(), make_quad());
    if (!primitive || !document.set_node_mesh(node.value(), mesh.value()) ||
        !document.add_scene_root(scene.value(), node.value())) {
        return 2;
    }
    const elf3d::DocumentStatistics source = document.statistics();
    if (source.vertices != 4U || source.indices != 6U || source.triangles != 2U) {
        return 3;
    }

    if (!document.replace_primitive(primitive.value(), make_reduced_triangle())) {
        return 4;
    }
    const auto reduced = document.primitive(primitive.value());
    const elf3d::DocumentStatistics result = document.statistics();
    if (!reduced || reduced.value().data.positions.size() != 3U ||
        reduced.value().data.indices.size() != 3U || result.vertices != 3U ||
        result.indices != 3U || result.triangles != 1U) {
        return 5;
    }
    if (reduced.value().bounds.minimum != elf3d::Float3{-1.0F, -1.0F, 0.0F} ||
        reduced.value().bounds.maximum != elf3d::Float3{1.0F, 1.0F, 0.0F}) {
        return 6;
    }
    return elf3d::validate_document(document.view()).has_errors() ? 7 : 0;
}
