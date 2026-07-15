#include <elf3d/model.h>

#include <cstdint>
#include <optional>
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

struct ProcessingFixture {
    elf3d::Document document;
    elf3d::PrimitiveId primitive;
};

[[nodiscard]] bool configure_processing_document(elf3d::Document& document,
                                                 elf3d::DocumentSceneId scene, elf3d::NodeId node,
                                                 elf3d::MeshId mesh) {
    return document.set_node_mesh(node, mesh) && document.add_scene_root(scene, node);
}

[[nodiscard]] std::optional<ProcessingFixture> make_processing_fixture() {
    elf3d::Document document;
    const auto scene = document.create_scene("Processing");
    const auto node = document.create_node("Reduced mesh");
    const auto mesh = document.create_mesh("Source quad");
    const auto material = document.create_material();
    if (!scene || !node || !mesh || !material) {
        return std::nullopt;
    }

    const auto primitive = document.create_primitive(mesh.value(), material.value(), make_quad());
    if (!primitive ||
        !configure_processing_document(document, scene.value(), node.value(), mesh.value())) {
        return std::nullopt;
    }
    return ProcessingFixture{std::move(document), primitive.value()};
}

[[nodiscard]] bool has_source_statistics(const elf3d::Document& document) {
    const elf3d::DocumentStatistics source = document.statistics();
    return source.vertices == 4U && source.indices == 6U && source.triangles == 2U;
}

[[nodiscard]] bool has_reduced_data(const elf3d::Result<elf3d::PrimitiveView>& reduced,
                                    const elf3d::DocumentStatistics& statistics) {
    return reduced && reduced.value().data.positions.size() == 3U &&
           reduced.value().data.indices.size() == 3U && statistics.vertices == 3U &&
           statistics.indices == 3U && statistics.triangles == 1U;
}

[[nodiscard]] bool has_reduced_bounds(const elf3d::PrimitiveView& reduced) {
    return reduced.bounds.minimum == elf3d::Float3{-1.0F, -1.0F, 0.0F} &&
           reduced.bounds.maximum == elf3d::Float3{1.0F, 1.0F, 0.0F};
}

[[nodiscard]] int verify_replacement(ProcessingFixture& fixture) {
    if (!fixture.document.replace_primitive(fixture.primitive, make_reduced_triangle())) {
        return 4;
    }
    const auto reduced = fixture.document.primitive(fixture.primitive);
    const elf3d::DocumentStatistics result = fixture.document.statistics();
    if (!has_reduced_data(reduced, result)) {
        return 5;
    }
    if (!has_reduced_bounds(reduced.value())) {
        return 6;
    }
    return elf3d::validate_document(fixture.document.view()).has_errors() ? 7 : 0;
}

} // namespace

int elf3d_model_processing_test() {
    std::optional<ProcessingFixture> fixture = make_processing_fixture();
    if (!fixture) {
        return 1;
    }
    if (!has_source_statistics(fixture->document)) {
        return 3;
    }
    return verify_replacement(*fixture);
}
