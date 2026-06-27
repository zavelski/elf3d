#include <elf3d/assets.h>
#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/measurement.h>
#include <elf3d/scene.h>

#include <array>
#include <cmath>

import elf.assets;
import elf.scene;
import elf.tool.clipping;

namespace {

[[nodiscard]] elf3d::SceneId scene_id(std::uint64_t value) noexcept {
    return elf3d::detail::SceneHandleAccess::create_scene(61, value);
}

[[nodiscard]] elf3d::scene::Storage make_scene() {
    elf3d::scene::Storage scene{scene_id(1)};
    const std::array<elf3d::VertexPositionNormal, 8> vertices{{
        {{-1.0F, -1.0F, -1.0F}, {0.0F, 1.0F, 0.0F}},
        {{1.0F, -1.0F, -1.0F}, {0.0F, 1.0F, 0.0F}},
        {{-1.0F, 1.0F, -1.0F}, {0.0F, 1.0F, 0.0F}},
        {{1.0F, 1.0F, -1.0F}, {0.0F, 1.0F, 0.0F}},
        {{-1.0F, -1.0F, 1.0F}, {0.0F, 1.0F, 0.0F}},
        {{1.0F, -1.0F, 1.0F}, {0.0F, 1.0F, 0.0F}},
        {{-1.0F, 1.0F, 1.0F}, {0.0F, 1.0F, 0.0F}},
        {{1.0F, 1.0F, 1.0F}, {0.0F, 1.0F, 0.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const elf3d::MeshHandle mesh = scene.create_mesh({vertices, indices}).value();
    const elf3d::MaterialHandle material = scene.create_material({}).value();
    static_cast<void>(scene.create_model(mesh, material));
    return scene;
}

[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::abs(left - right) <= tolerance;
}

} // namespace

int main() {
    elf3d::tools::clipping::ClippingController controller;
    if (controller.snapshot().revision != 0 || controller.snapshot().box_count != 0) {
        return 1;
    }

    elf3d::SectionPlane plane;
    plane.enabled = true;
    plane.normal = {1.0F, 0.0F, 0.0F};
    if (!controller.set_section_plane(plane) || controller.snapshot().revision != 1) {
        return 2;
    }
    if (!controller.set_section_plane(plane) || controller.snapshot().revision != 1) {
        return 3;
    }
    plane.retained_half_space = elf3d::PlaneHalfSpace::negative;
    if (!controller.set_section_plane(plane) ||
        controller.snapshot().section_plane.retained_half_space !=
            elf3d::PlaneHalfSpace::negative) {
        return 4;
    }
    controller.clear_section_plane();
    if (controller.snapshot().section_plane.enabled || controller.snapshot().revision != 3) {
        return 5;
    }

    const elf3d::ClippingBox first{{-1.0F, -1.0F, -1.0F}, {0.0F, 1.0F, 1.0F}, true};
    const elf3d::ClippingBox second{{1.0F, -1.0F, -1.0F}, {2.0F, 1.0F, 1.0F}, false};
    const elf3d::ClippingBox third{{3.0F, -1.0F, -1.0F}, {4.0F, 1.0F, 1.0F}, true};
    if (controller.add_box(first).value() != 0 || controller.add_box(second).value() != 1 ||
        controller.add_box(third).value() != 2 ||
        controller.add_box(first).error().code() != elf3d::ErrorCode::clipping_box_limit_exceeded ||
        controller.snapshot().box_count != 3 || controller.snapshot().boxes[1].enabled) {
        return 6;
    }
    if (!controller.remove_box(1) || controller.snapshot().box_count != 2 ||
        controller.snapshot().boxes[1] != third) {
        return 7;
    }
    if (controller.remove_box(2).error().code() != elf3d::ErrorCode::invalid_clipping_box_index) {
        return 8;
    }

    elf3d::scene::Storage scene = make_scene();
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(scene, std::nullopt).value();
    if (!controller.reset_box_to_visible_bounds(scene, visibility, 0)) {
        return 9;
    }
    const elf3d::ClippingBox reset = controller.snapshot().boxes[0];
    if (!nearly_equal(reset.minimum.x, -1.0F) || !nearly_equal(reset.maximum.x, 1.0F)) {
        return 10;
    }
    controller.clear_boxes();
    if (controller.snapshot().box_count != 0) {
        return 11;
    }
    if (controller.add_box_from_visible_bounds(scene, visibility).value() != 0) {
        return 12;
    }

    plane.enabled = true;
    plane.retained_half_space = elf3d::PlaneHalfSpace::positive;
    if (!controller.set_section_plane(plane)) {
        return 13;
    }
    const elf3d::clipping::ClippingFilter filter = controller.filter().value();
    const elf3d::Bounds3 clipped =
        elf3d::tools::clipping::visible_bounds(scene, visibility, filter);
    if (!clipped.is_valid || !nearly_equal(clipped.minimum.x, 0.0F) ||
        !nearly_equal(clipped.maximum.x, 1.0F)) {
        return 14;
    }
    const elf3d::tools::clipping::ClippingOverlay overlay =
        controller.overlay(scene.world_bounds()).value();
    if (overlay.line_count != 16) {
        return 15;
    }
    if (!controller.set_helpers_visible(false) ||
        controller.overlay(scene.world_bounds()).value().line_count != 0) {
        return 16;
    }
    controller.clear();
    if (controller.snapshot().section_plane.enabled || controller.snapshot().box_count != 0) {
        return 17;
    }
    return 0;
}
