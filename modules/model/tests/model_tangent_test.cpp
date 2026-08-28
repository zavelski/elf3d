#include <elf3d/model.h>

#include <array>

import elf.model;

namespace {

[[nodiscard]] elf3d::PrimitiveData triangle_data() {
    elf3d::PrimitiveData data;
    data.positions = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    data.normals = {{0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}};
    data.indices = {0U, 1U, 2U};
    return data;
}

[[nodiscard]] bool rejected_with(const elf3d::Result<elf3d::PrimitiveId>& result,
                                 elf3d::ErrorCode code) {
    return !result && result.error().code() == code;
}

[[nodiscard]] bool valid_tangent_view(const elf3d::Document& document,
                                      const elf3d::Result<elf3d::PrimitiveId>& created) {
    if (!created) {
        return false;
    }
    const auto view = document.primitive(created.value());
    return view && view.value().data.tangents.size() == 3U &&
           view.value().data.tangents[2].w == -1.0F;
}

} // namespace

int elf3d_model_tangent_test() {
    elf3d::Document document;
    const auto mesh = document.create_mesh("tangent validation");
    const auto material = document.create_material({});
    if (!mesh || !material) {
        return 1;
    }

    elf3d::PrimitiveData valid = triangle_data();
    valid.tangents = {
        {1.0F, 0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F, -1.0F}};
    const auto created = document.create_primitive(mesh.value(), material.value(), valid.view());

    elf3d::PrimitiveData mismatched = triangle_data();
    mismatched.tangents = {{1.0F, 0.0F, 0.0F, 1.0F}};
    const auto mismatched_result =
        document.create_primitive(mesh.value(), material.value(), mismatched.view());

    elf3d::PrimitiveData invalid = triangle_data();
    invalid.tangents = {
        {0.0F, 0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F, 1.0F}};
    const auto invalid_result =
        document.create_primitive(mesh.value(), material.value(), invalid.view());

    const std::array<bool, 3> checks{
        valid_tangent_view(document, created),
        rejected_with(mismatched_result, elf3d::ErrorCode::invalid_mesh_data),
        rejected_with(invalid_result, elf3d::ErrorCode::invalid_accessor),
    };
    for (const bool passed : checks) {
        if (!passed) {
            return 2;
        }
    }
    return 0;
}
