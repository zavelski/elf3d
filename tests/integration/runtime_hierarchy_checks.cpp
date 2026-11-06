#include "runtime_hierarchy_checks.h"

#include <elf3d/elf3d.h>

#include <array>
#include <cstddef>
#include <new>
#include <string>
#include <utility>

namespace elf3d::test {
namespace {
[[nodiscard]] Error failure(const char* message)
{
    return {ErrorCode::invalid_argument, message};
}
struct Fixture {
    EntityId root;
    EntityId camera;
    EntityId model;
    std::size_t count = 0;
    std::string name;
};

[[nodiscard]] Result<EntityId> make_model(Scene& scene, EntityId parent)
{
    constexpr std::array<VertexPositionNormal, 3> vertices{
        VertexPositionNormal{{0, 0, 0}, {0, 0, 1}}, VertexPositionNormal{{1, 0, 0}, {0, 0, 1}},
        VertexPositionNormal{{0, 1, 0}, {0, 0, 1}}};
    constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
    const auto mesh = scene.create_mesh(MeshDataView{vertices, indices});
    const auto material = scene.create_material({});
    if (!mesh || !material) {
        return failure("Cannot create hierarchy model components");
    }
    const auto model = scene.create_model_entity(mesh.value(), material.value());
    if (!model || !scene.set_parent(model.value(), parent)) {
        return failure("Cannot attach hierarchy model entity");
    }
    return model.value();
}

[[nodiscard]] Result<void> make_children(Scene& scene, EntityId parent)
{
    for (std::size_t index = 0; index < 4096; ++index) {
        const auto child = scene.create_entity();
        if (!child || !scene.set_parent(child.value(), parent) ||
            !scene.set_entity_name(child.value(), "Duplicate name")) {
            return failure("Cannot construct wide runtime hierarchy fixture");
        }
        if (index == 31 && !scene.destroy_entity(child.value())) {
            return failure("Cannot create the hierarchy slot hole");
        }
    }
    return {};
}

[[nodiscard]] Result<Fixture> populate(Scene& scene)
{
    const auto root = scene.create_entity();
    if (!root) {
        return root.error();
    }
    Fixture fixture;
    fixture.root = root.value();
    fixture.name = std::string(65536, 'x') + " UTF-8 \xD0\xBC\xD0\xB5\xD1\x88";
    if (const auto named = scene.set_entity_name(fixture.root, fixture.name); !named) {
        return named.error();
    }
    if (const auto children = make_children(scene, fixture.root); !children) {
        return children.error();
    }
    const auto camera = scene.create_perspective_camera_entity({});
    if (!camera) {
        return camera.error();
    }
    fixture.camera = camera.value();
    const auto model = make_model(scene, fixture.root);
    if (!model) {
        return model.error();
    }
    fixture.model = model.value();
    fixture.count = 4098; // root + 4095 surviving children + model + camera
    return fixture;
}

[[nodiscard]] Result<void> compare_captures(const SceneHierarchySnapshot& incremental,
                                            const SceneHierarchySnapshot& synchronous)
{
    if (!synchronous.ready() || incremental.size() != synchronous.size()) {
        return failure("Synchronous hierarchy capture did not drive the common builder");
    }
    for (std::size_t index = 0; index < incremental.size(); ++index) {
        const auto item = incremental.item_at(index);
        const auto name = incremental.name_at(index);
        if (!item || !name || item.value() != synchronous.item_at(index).value() ||
            name.value() != synchronous.name_at(index).value()) {
            return failure("Bounded hierarchy capture changed traversal order or metadata");
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_pending(Scene& scene, SceneHierarchySnapshot& snapshot)
{
    if (snapshot.ready() || snapshot.size() != 0 || snapshot.item_at(0) || snapshot.name_at(0) ||
        snapshot.advance(scene, 0)) {
        return failure("A pending runtime capture exposed rows or accepted a zero budget");
    }
    return {};
}

[[nodiscard]] Result<void> verify_shape(const SceneHierarchySnapshot& snapshot,
                                        const Fixture& fixture)
{
    const auto root = snapshot.item_at(0).value();
    const auto model = snapshot.item_at(fixture.count - 2).value();
    const auto camera = snapshot.item_at(fixture.count - 1).value();
    if (root.child_count != 4096 || root.depth != 0 || model.entity != fixture.model ||
        !model.has_model || !model.renderable || camera.entity != fixture.camera ||
        !camera.has_camera) {
        return failure("Runtime capture lost independently expected component/forest metadata");
    }
    return {};
}

[[nodiscard]] Result<void> drive_capture(Scene& scene, SceneHierarchySnapshot& snapshot,
                                         const Fixture& fixture)
{
    const auto calls_limit = fixture.count * 8 + fixture.name.size() / 1024 + 64;
    std::size_t calls = 0;
    while (!snapshot.ready() && calls < calls_limit) {
        if (const auto advanced = snapshot.advance(scene, 1); !advanced) {
            return advanced.error();
        }
        ++calls;
    }
    if (!snapshot.ready() || calls < fixture.count * 2 || snapshot.size() != fixture.count ||
        snapshot.name_at(0).value() != fixture.name) {
        return failure("Runtime capture did not respect bounded structural/name advancement");
    }
    return verify_shape(snapshot, fixture);
}

[[nodiscard]] Result<SceneHierarchySnapshot> capture(Scene& scene, const Fixture& fixture)
{
    auto incremental = SceneHierarchySnapshot::create(scene);
    if (!incremental) {
        return incremental.error();
    }
    if (const auto checked = validate_pending(scene, incremental.value()); !checked) {
        return checked.error();
    }
    if (const auto driven = drive_capture(scene, incremental.value(), fixture); !driven) {
        return driven.error();
    }
    const auto synchronous = scene.hierarchy_snapshot();
    if (!synchronous) {
        return synchronous.error();
    }
    if (const auto compared = compare_captures(incremental.value(), synchronous.value());
        !compared) {
        return compared.error();
    }
    return std::move(incremental).value();
}

[[nodiscard]] Result<void> verify_invalidating_changes(Scene& scene, const Fixture& fixture)
{
    auto pending = SceneHierarchySnapshot::create(scene);
    if (!pending) {
        return pending.error();
    }
    if (!scene.set_entity_local_visibility(fixture.root, false) ||
        pending.value().matches_scene(scene) || pending.value().advance(scene, 1)) {
        return failure("Visibility mutation did not invalidate pending runtime capture");
    }
    auto renamed = SceneHierarchySnapshot::create(scene);
    if (!renamed || !scene.set_entity_name(fixture.root, "Changed") ||
        renamed.value().matches_scene(scene) || renamed.value().advance(scene, 1)) {
        return failure("Name mutation did not invalidate pending runtime capture");
    }
    return {};
}

[[nodiscard]] Result<void> verify_changes(Scene& scene, const Fixture& fixture)
{
    auto pending = SceneHierarchySnapshot::create(scene);
    if (!pending || !pending.value().advance(scene, 1)) {
        return failure("Cannot begin runtime revision capture");
    }
    const auto moved = scene.set_local_transform(fixture.camera, Transform{{1, 2, 3}});
    if (!moved || !pending.value().matches_scene(scene) || !pending.value().advance(scene, 1)) {
        return failure("Camera motion invalidated structural runtime capture");
    }
    return verify_invalidating_changes(scene, fixture);
}

[[nodiscard]] Result<void> verify_empty(Scene& scene)
{
    SceneHierarchySnapshot empty;
    if (empty.ready() || empty.size() != 0 || empty.item_at(0) || empty.name_at(0)) {
        return failure("Default runtime capture is not empty");
    }
    const auto zero = scene.hierarchy_snapshot();
    if (!zero || !zero.value().ready() || zero.value().size() != 0) {
        return failure("Empty runtime capture is not complete");
    }
    return {};
}

[[nodiscard]] Result<void> verify_foreign_and_names(Scene& scene, const Scene& foreign,
                                                    const Fixture& fixture,
                                                    SceneHierarchySnapshot& snapshot)
{
    if (snapshot.matches_scene(foreign) || snapshot.advance(foreign, 1)) {
        return failure("Foreign Scene accepted a runtime capture");
    }
    if (const auto changed = verify_changes(scene, fixture); !changed) {
        return changed.error();
    }
    if (snapshot.name_at(0).value() != fixture.name) {
        return failure("Scene mutation changed owned hierarchy names");
    }
    return {};
}

[[nodiscard]] Result<void> verify_contract(Engine& engine)
{
    auto scene = engine.create_scene();
    auto foreign = engine.create_scene();
    if (!scene || !foreign) {
        return failure("Cannot create runtime capture owners");
    }
    if (const auto empty = verify_empty(*scene.value()); !empty) {
        return empty.error();
    }
    const auto fixture = populate(*scene.value());
    if (!fixture) {
        return fixture.error();
    }
    auto snapshot = capture(*scene.value(), fixture.value());
    if (!snapshot) {
        return snapshot.error();
    }
    if (const auto changed = verify_foreign_and_names(*scene.value(), *foreign.value(),
                                                      fixture.value(), snapshot.value());
        !changed) {
        return changed.error();
    }
    scene.value().reset();
    if (snapshot.value().size() != fixture.value().count ||
        snapshot.value().name_at(0).value() != fixture.value().name) {
        return failure("Runtime hierarchy metadata depended on the retired Scene");
    }
    return {};
}
} // namespace

Result<void> verify_runtime_hierarchy_contract(Engine& engine) noexcept
{
    try {
        return verify_contract(engine);
    } catch (const std::bad_alloc&) {
        fatal_error("Runtime hierarchy fixture allocation failed");
    } catch (...) {
        fatal_error("Unexpected runtime hierarchy fixture exception");
    }
}
} // namespace elf3d::test
