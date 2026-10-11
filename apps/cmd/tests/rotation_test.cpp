#include "fixture.h"
#include "rotate_document.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {

[[nodiscard]] bool near(elf3d::Float3 first, elf3d::Float3 second)
{
    return std::abs(first.x - second.x) < 0.0001F && std::abs(first.y - second.y) < 0.0001F &&
           std::abs(first.z - second.z) < 0.0001F;
}

[[nodiscard]] elf3d::Float3 transform_point(elf3d::Float4x4 matrix, elf3d::Float3 point)
{
    const auto& m = matrix.elements;
    return {m[0] * point.x + m[4] * point.y + m[8] * point.z + m[12],
            m[1] * point.x + m[5] * point.y + m[9] * point.z + m[13],
            m[2] * point.x + m[6] * point.y + m[10] * point.z + m[14]};
}

[[nodiscard]] elf3d::Bounds3 world_bounds(const elf3d::Document& document, elf3d::Float4x4 root,
                                          elf3d::Float4x4 child)
{
    const auto positions = document.primitive_at(0).value().data.positions;
    const auto first = transform_point(root, transform_point(child, positions.front()));
    elf3d::Bounds3 bounds{first, first};
    for (const auto position : positions) {
        const auto world = transform_point(root, transform_point(child, position));
        bounds.minimum = {std::min(bounds.minimum.x, world.x), std::min(bounds.minimum.y, world.y),
                          std::min(bounds.minimum.z, world.z)};
        bounds.maximum = {std::max(bounds.maximum.x, world.x), std::max(bounds.maximum.y, world.y),
                          std::max(bounds.maximum.z, world.z)};
    }
    return bounds;
}

[[nodiscard]] bool expected_root(elf3d::Float4x4 root)
{
    const auto& m = root.elements;
    return near({m[12], m[13], m[14]}, {1, 21, 30}) && near({m[0], m[1], m[2]}, {-2, 0, 0}) &&
           near({m[4], m[5], m[6]}, {0, -2, 0});
}

[[nodiscard]] bool expected_nodes(const elf3d::Document& document, elf3d::Float4x4 root,
                                  elf3d::Float4x4 child_before, elf3d::DocumentSceneId first)
{
    const auto camera = document.node_at(3).value().local_matrix;
    return expected_root(root) && near(transform_point(camera, {}), {21, 11, 0}) &&
           near({camera.elements[0], camera.elements[1], camera.elements[2]}, {0, 1, 0}) &&
           document.node_at(1).value().local_matrix == child_before &&
           document.default_scene() == first;
}

[[nodiscard]] bool expected_world_geometry(const elf3d::Document& document, elf3d::Float4x4 root,
                                           elf3d::Float4x4 child)
{
    // All instances together have world bounds [0,0,0] .. [10,32,30], center (5,16,15).
    return near(transform_point(root, transform_point(child, {})), {-9, 21, 30}) &&
           near(transform_point(document.node_at(2).value().local_matrix, {5, 16, 15}),
                {5, 16, 15});
}

int valid_rotation_test(elf3d::Document& document)
{
    const auto statistics = document.statistics();
    const auto child_before = document.node_at(1).value().local_matrix;
    const auto bounds_before = document.bounds();
    const auto first = document.scene_at(0).value().id;
    if (!document.set_default_scene(first) || !elf3d::cmd::rotate_document(document, {0, 0, 90})) {
        return 2;
    }
    const auto root = document.node_at(0).value().local_matrix;
    if (!expected_nodes(document, root, child_before, first)) {
        return 3;
    }
    const auto bounds = world_bounds(document, root, child_before);
    if (!near(bounds.minimum, {-11, 19, 30}) || !near(bounds.maximum, {-9, 21, 30})) {
        return 4;
    }
    if (!expected_world_geometry(document, root, child_before)) {
        return 5;
    }
    return document.bounds() == bounds_before && document.statistics() == statistics &&
                   !elf3d::validate_document(document.view()).has_errors()
               ? 0
               : 6;
}

int axis_rotation_test()
{
    struct Case {
        elf3d::Float3 degrees;
        elf3d::Float3 root_position;
        elf3d::Float3 first_column;
    };
    const std::array cases{
        Case{{90, 0, 0}, {10, 1, 19}, {0, 0, 2}}, Case{{0, 90, 0}, {20, 20, 10}, {0, 2, 0}},
        Case{{0, 0, -90}, {9, 11, 30}, {2, 0, 0}}, Case{{90, 90, 0}, {9, 1, 10}, {2, 0, 0}},
        Case{{0, 0, 22.5F}, {8.0886639F, 21.6089353F, 30}, {-0.7653669F, 1.8477591F, 0}}};
    for (const auto& sample : cases) {
        auto fixture = make_fixture();
        if (!fixture || !elf3d::cmd::rotate_document(fixture.value(), sample.degrees)) {
            return 7;
        }
        const auto m = fixture.value().node_at(0).value().local_matrix.elements;
        if (!near({m[12], m[13], m[14]}, sample.root_position) ||
            !near({m[0], m[1], m[2]}, sample.first_column)) {
            return 8;
        }
    }
    return 0;
}

int camera_center_test()
{
    auto fixture = make_fixture();
    if (!fixture) {
        return 9;
    }
    elf3d::Float4x4 camera;
    camera.elements[12] = 1000;
    const auto id = fixture.value().node_at(3).value().id;
    if (!fixture.value().set_node_matrix(id, camera) ||
        !elf3d::cmd::rotate_document(fixture.value(), {0, 0, 90})) {
        return 9;
    }
    return expected_root(fixture.value().node_at(0).value().local_matrix) &&
                   near(transform_point(fixture.value().node_at(3).value().local_matrix, {}),
                        {21, 1011, 0})
               ? 0
               : 9;
}

int invalid_rotation_test(elf3d::Document& document)
{
    const auto root = document.node_at(0).value().local_matrix;
    if (!elf3d::cmd::rotate_document(document, {}) ||
        !elf3d::cmd::rotate_document(document, {360, -720, 1080}) ||
        document.node_at(0).value().local_matrix != root ||
        elf3d::cmd::rotate_document(document, {std::numeric_limits<float>::infinity(), 0, 0})) {
        return 10;
    }
    elf3d::Float4x4 huge;
    huge.elements[12] = std::numeric_limits<float>::max();
    huge.elements[13] = std::numeric_limits<float>::max();
    const auto id = document.node_at(3).value().id;
    if (!document.set_node_matrix(id, huge)) {
        return 11;
    }
    const auto overflow = elf3d::cmd::rotate_document(document, {0, 0, -45});
    return !overflow && overflow.error().code() == elf3d::ErrorCode::invalid_transform_matrix ? 0
                                                                                              : 11;
}

int unsupported_rotation_test()
{
    elf3d::Document empty;
    if (!elf3d::cmd::rotate_document(empty, {}) || elf3d::cmd::rotate_document(empty, {0, 90, 0})) {
        return 12;
    }
    auto fixture = make_fixture();
    if (!fixture) {
        return 13;
    }
    const auto id = fixture.value().node_at(0).value().id;
    auto matrix = fixture.value().node_at(0).value().local_matrix;
    matrix.elements[3] = 0.5F;
    if (!fixture.value().set_node_matrix(id, matrix)) {
        return 13;
    }
    return elf3d::cmd::rotate_document(fixture.value(), {0, 90, 0}) ? 13 : 0;
}

} // namespace

int rotation_test()
{
    auto fixture = make_fixture();
    if (!fixture) {
        return 1;
    }
    if (const int result = valid_rotation_test(fixture.value()); result != 0) {
        return result;
    }
    if (const int result = invalid_rotation_test(fixture.value()); result != 0) {
        return result;
    }
    if (const int result = axis_rotation_test(); result != 0) {
        return result;
    }
    if (const int result = camera_center_test(); result != 0) {
        return result;
    }
    return unsupported_rotation_test();
}
