#include "primitive_visibility_checks.h"

#include <elf3d/elf3d.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace elf3d::test {
namespace {
[[nodiscard]] Error failure(const char* text)
{
    return {ErrorCode::invalid_argument, text};
}
[[nodiscard]] PrimitiveData quad(float center)
{
    PrimitiveData data;
    data.positions = {
        {center - 1, -1, 0}, {center + 1, -1, 0}, {center + 1, 1, 0}, {center - 1, 1, 0}};
    data.normals.assign(4, {0, 0, 1});
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}
struct Fixture {
    std::unique_ptr<Scene> scene;
    std::unique_ptr<Viewport> first;
    std::unique_ptr<Viewport> second;
    EntityId camera;
    std::array<EntityId, 2> entities;
    std::array<PrimitiveId, 2> primitives;
    PrimitiveId unused;
};
[[nodiscard]] Result<Fixture> make_fixture(Engine& engine)
{
    Document document;
    const auto selected = document.create_scene("Primitive visibility").value();
    const auto mesh = document.create_mesh("Shared mesh").value();
    ModelMaterialDescription description;
    description.double_sided = true;
    const auto material = document.create_material(description).value();
    Fixture f;
    f.primitives = {document.create_primitive(mesh, material, quad(-2)).value(),
                    document.create_primitive(mesh, material, quad(2)).value()};
    const auto unused = document.create_mesh("Unused mesh").value();
    f.unused = document.create_primitive(unused, material, quad(20)).value();
    std::array<NodeId, 2> nodes;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        nodes[i] = document.create_node("Shared instance").value();
        Float4x4 matrix;
        matrix.elements[12] = static_cast<float>(i) * 8;
        if (!document.set_node_matrix(nodes[i], matrix) ||
            !document.set_node_mesh(nodes[i], mesh) || !document.add_scene_root(selected, nodes[i]))
            return failure("Cannot prepare shared primitive instances");
    }
    auto created = engine.create_scene_from_document(std::move(document), selected);
    if (!created)
        return created.error();
    f.scene = std::move(created).value();
    f.camera = f.scene->create_perspective_camera_entity({}).value();
    for (std::size_t i = 0; i < nodes.size(); ++i)
        f.entities[i] = *f.scene->entity_for_document_node(nodes[i]).value();
    f.first = engine.create_viewport({96, 96}).value();
    f.second = engine.create_viewport({96, 96}).value();
    f.first->set_render_shading_mode(RenderShadingMode::unlit);
    f.second->set_render_shading_mode(RenderShadingMode::unlit);
    if (!f.first->fit_to_scene(*f.scene, f.camera) || !f.second->fit_to_scene(*f.scene, f.camera))
        return failure("Cannot frame the primitive-mask fixture");
    return f;
}
[[nodiscard]] Result<void> rendered(Fixture& f, std::uint64_t first, std::uint64_t second)
{
    if (!f.first->render(*f.scene, f.camera) || !f.second->render(*f.scene, f.camera))
        return failure("Primitive visibility rendering failed");
    if (f.first->render_statistics().triangles != first ||
        f.second->render_statistics().triangles != second)
        return failure("Primitive masks changed the wrong draw set or leaked to another viewport");
    return {};
}
[[nodiscard]] Result<void> bounds(Fixture& f, float minimum, float maximum)
{
    const auto box = f.first->unclipped_visible_bounds(*f.scene);
    if (!box || !box.value())
        return failure("Masked visible bounds are unavailable");
    if (std::abs(box.value()->minimum.x - minimum) > .0001F ||
        std::abs(box.value()->maximum.x - maximum) > .0001F)
        return failure("Primitive masks did not restrict visible bounds");
    return {};
}
[[nodiscard]] Result<void> picked(Fixture& f, float center,
                                  std::optional<PrimitiveInstance> expected)
{
    const auto point = f.first->project_world_to_viewport(*f.scene, f.camera, {center, 0, 0});
    if (!point)
        return point.error();
    const auto hit = f.first->pick(*f.scene, f.camera, point.value().position_pixels);
    if (!hit || hit.value().has_value() != expected.has_value())
        return failure("Primitive masks did not restrict picking");
    if (expected) {
        const auto primitive = f.scene->document_primitive(*hit.value());
        if (!primitive || primitive.value() != expected->primitive ||
            hit.value()->entity != expected->entity)
            return failure("Picked runtime group did not map to the selected canonical instance");
    }
    return {};
}
[[nodiscard]] Result<SurfaceAnchor> left_anchor(Fixture& f)
{
    const auto point = f.first->project_world_to_viewport(*f.scene, f.camera, {-2, 0, 0});
    if (!point)
        return point.error();
    const auto hit = f.first->pick(*f.scene, f.camera, point.value().position_pixels);
    if (!hit || !hit.value())
        return failure("Cannot anchor the initially visible primitive");
    return f.scene->create_surface_anchor(*hit.value());
}
[[nodiscard]] Result<void> anchor_visibility(Fixture& f, const SurfaceAnchor& anchor)
{
    const auto resolved = f.scene->resolve_surface_anchor(anchor);
    if (!resolved)
        return resolved.error();
    const auto first = f.first->surface_anchor_visible(*f.scene, resolved.value());
    const auto second = f.second->surface_anchor_visible(*f.scene, resolved.value());
    if (!first || first.value() || !second || !second.value())
        return failure("Primitive masks and measurement-anchor visibility disagree");
    return {};
}
[[nodiscard]] Result<void> hide_one_instance(Fixture& f)
{
    const std::array hidden{PrimitiveInstance{f.entities[0], f.primitives[0]}};
    const auto anchor = left_anchor(f);
    if (!anchor)
        return anchor.error();
    const auto before = f.first->render_revision();
    if (!f.first->set_primitive_local_visibility(*f.scene, hidden, false) ||
        f.first->render_revision() == before)
        return failure("Hiding a primitive did not invalidate the viewport frame");
    const auto after = f.first->render_revision();
    if (!f.first->set_primitive_local_visibility(*f.scene, hidden, false) ||
        f.first->render_revision() != after)
        return failure("Repeated visibility assignment invalidated an unchanged frame");
    if (const auto r = rendered(f, 6, 8); !r)
        return r.error();
    if (const auto checked = anchor_visibility(f, anchor.value()); !checked)
        return checked.error();
    if (const auto p = picked(f, -2, std::nullopt); !p)
        return p.error();
    return picked(f, 6, PrimitiveInstance{f.entities[1], f.primitives[0]});
}
[[nodiscard]] Result<void> clipped_visibility(Fixture& f)
{
    if (const auto full = bounds(f, 1, 11); !full)
        return full.error();
    const SectionPlane plane{{4, 0, 0}, {-1, 0, 0}, PlaneHalfSpace::positive, true};
    if (!f.first->set_section_plane(plane))
        return failure("Cannot apply the view-local clipping plane");
    const auto box = f.first->visible_bounds(*f.scene);
    if (!box || !box.value())
        return failure("Masked clipped bounds are unavailable");
    if (std::abs(box.value()->minimum.x - 1) > .0001F ||
        std::abs(box.value()->maximum.x - 3) > .0001F)
        return failure("Clipped bounds retained a hidden primitive");
    auto result = rendered(f, 2, 8);
    f.first->clear_section_plane();
    return result;
}
[[nodiscard]] Result<void> isolate_one_instance(Fixture& f)
{
    const std::array keys{PrimitiveInstance{f.entities[1], f.primitives[1]}};
    if (!f.first->isolate_primitives(*f.scene, keys) || !f.first->is_isolating() ||
        f.first->isolated_primitive_count() != 1 || f.first->isolated_entity_count() != 0)
        return failure("Primitive isolation lost its explicit scope");
    if (const auto r = rendered(f, 2, 8); !r)
        return r.error();
    if (const auto b = bounds(f, 9, 11); !b)
        return b.error();
    if (const auto p = picked(f, 6, std::nullopt); !p)
        return p.error();
    return picked(f, 10, keys[0]);
}
[[nodiscard]] Result<void> hidden_by_entity(Fixture& f, PrimitiveInstance key)
{
    const auto local = f.first->primitive_local_visibility(*f.scene, key);
    const auto effective = f.first->primitive_effective_visibility(*f.scene, key);
    const auto box = f.first->visible_bounds(*f.scene);
    if (!local || !local.value() || !effective || effective.value() || !box || box.value())
        return failure("Local, inherited and isolation visibility were conflated");
    return {};
}
[[nodiscard]] Result<void> inherited_visibility(Fixture& f)
{
    const PrimitiveInstance key{f.entities[1], f.primitives[1]};
    if (!f.scene->set_entity_local_visibility(key.entity, false))
        return failure("Cannot hide the primitive's model entity");
    if (const auto checked = hidden_by_entity(f, key); !checked)
        return checked.error();
    if (const auto r = rendered(f, 0, 4); !r)
        return r.error();
    if (!f.scene->set_entity_local_visibility(key.entity, true))
        return failure("Cannot restore model visibility");
    f.first->clear_isolation();
    if (const auto r = rendered(f, 6, 8); !r)
        return r.error();
    f.first->clear_primitive_visibility();
    return rendered(f, 8, 8);
}
[[nodiscard]] PrimitiveId foreign_primitive()
{
    Document document;
    const auto mesh = document.create_mesh().value();
    const auto material = document.create_material().value();
    return document.create_primitive(mesh, material, quad(0)).value();
}
[[nodiscard]] Result<void> reject_foreign(Fixture& f)
{
    const std::array keys{PrimitiveInstance{f.entities[0], foreign_primitive()}};
    if (f.first->set_primitive_local_visibility(*f.scene, keys, false) ||
        f.first->primitive_local_visibility(*f.scene, keys[0]) ||
        f.first->isolate_primitives(*f.scene, keys))
        return failure("A foreign/stale primitive identity changed the view");
    return {};
}
[[nodiscard]] Result<void> verify_masks(Fixture& f)
{
    if (const auto hidden = hide_one_instance(f); !hidden)
        return hidden.error();
    if (const auto clipped = clipped_visibility(f); !clipped)
        return clipped.error();
    if (const auto isolated = isolate_one_instance(f); !isolated)
        return isolated.error();
    return inherited_visibility(f);
}
[[nodiscard]] Result<void> reject_invalid(Fixture& f)
{
    const std::array invalid{PrimitiveInstance{f.entities[0], f.primitives[0]},
                             PrimitiveInstance{f.entities[1], f.unused}};
    const auto revision = f.first->render_revision();
    if (f.first->set_primitive_local_visibility(*f.scene, invalid, false) ||
        f.first->isolate_primitives(*f.scene, invalid) || f.first->is_isolating() ||
        f.first->render_revision() != revision)
        return failure("Invalid primitive input partially changed view state");
    return rendered(f, 8, 8);
}
[[nodiscard]] Result<std::vector<std::uint8_t>> highlight_image(Fixture& f, bool highlight)
{
    ViewportRenderOptions options;
    options.shading_mode = RenderShadingMode::unlit;
    if (highlight)
        options.primitive_highlight = SelectionHighlight{{1, 0, 0, 1}, 1};
    if (const auto r = f.first->render(*f.scene, f.camera, options); !r)
        return r.error();
    std::vector<std::uint8_t> pixels(96 * 96 * 4);
    if (const auto r = f.first->read_color_pixels(pixels); !r)
        return r.error();
    return pixels;
}
[[nodiscard]] Result<std::array<std::uint8_t, 3>>
center_pixel(Fixture& f, std::span<const std::uint8_t> pixels, float center)
{
    const auto point = f.first->project_world_to_viewport(*f.scene, f.camera, {center, 0, 0});
    if (!point)
        return point.error();
    const auto position = point.value().position_pixels;
    if (position.x < 0 || position.x >= 96 || position.y < 0 || position.y >= 96)
        return failure("Highlight sample projects outside the fixture viewport");
    // Readback is bottom-up; the public projected point has a top-left origin.
    const auto x = static_cast<std::size_t>(position.x);
    const auto y = 95 - static_cast<std::size_t>(position.y);
    const auto offset = (y * 96 + x) * 4;
    return std::array{pixels[offset], pixels[offset + 1], pixels[offset + 2]};
}
[[nodiscard]] Result<void> highlight_scope(Fixture& f)
{
    const auto ordinary = highlight_image(f, false);
    const auto highlighted = highlight_image(f, true);
    if (!ordinary || !highlighted)
        return failure("Cannot capture primitive selection highlighting");
    const auto selected = center_pixel(f, highlighted.value(), -2);
    if (!selected || selected.value()[0] < 150 || selected.value()[1] > 30)
        return failure("Selected canonical primitive did not receive the red highlight");
    for (const float center : {2.0F, 6.0F, 10.0F}) {
        const auto before = center_pixel(f, ordinary.value(), center);
        const auto after = center_pixel(f, highlighted.value(), center);
        if (!before || !after || before.value() != after.value())
            return failure("Primitive highlight leaked to a sibling or another shared instance");
    }
    return {};
}
[[nodiscard]] Result<void> selection_rejection(Fixture& f, SelectionSnapshot accepted)
{
    const std::array invalid{PrimitiveInstance{f.entities[0], f.primitives[0]},
                             PrimitiveInstance{f.entities[1], f.unused}};
    const std::array foreign{PrimitiveInstance{f.entities[0], foreign_primitive()}};
    if (f.first->set_selected_primitives(*f.scene, invalid) ||
        f.first->set_selected_primitives(*f.scene, foreign) ||
        f.first->selection_snapshot() != accepted)
        return failure("Invalid selection input changed the previously accepted selection");
    return {};
}
[[nodiscard]] Result<void> primitive_selection_identity(Fixture& f)
{
    const PrimitiveInstance selected{f.entities[0], f.primitives[0]};
    const std::array duplicated{selected, selected};
    if (!f.first->set_selected_primitives(*f.scene, duplicated) ||
        f.first->selected_primitive_count() != 1 || !f.first->has_selection() ||
        f.first->selected_entity() || f.first->selection_hit())
        return failure("Primitive selection was converted to entity selection");
    const auto key = f.first->selected_primitive_at(0);
    if (!key || key.value() != selected || f.first->selected_primitive_at(1))
        return failure("Primitive selection enumeration lost canonical identity");
    return {};
}
[[nodiscard]] Result<void> primitive_selection(Fixture& f)
{
    if (const auto checked = primitive_selection_identity(f); !checked)
        return checked.error();
    const std::array duplicated{PrimitiveInstance{f.entities[0], f.primitives[0]}};
    const auto accepted = f.first->selection_snapshot();
    if (!f.first->set_selected_primitives(*f.scene, duplicated) ||
        f.first->selection_snapshot() != accepted)
        return failure("Repeated selection invalidated an unchanged retained frame");
    if (const auto checked = selection_rejection(f, accepted); !checked)
        return checked.error();
    return highlight_scope(f);
}
[[nodiscard]] Result<void> clear_primitive_selection(Fixture& f)
{
    const auto point = f.first->project_world_to_viewport(*f.scene, f.camera, {2, 0, 0});
    if (!point)
        return point.error();
    if (!f.first->select_at(*f.scene, f.camera, point.value().position_pixels) ||
        f.first->selected_primitive_count() != 0 || !f.first->selected_entity())
        return failure("Object picking did not replace primitive selection");
    f.first->clear_selection();
    if (f.first->has_selection() || f.first->selection_snapshot().primitive_count != 0 ||
        f.second->has_selection())
        return failure("Clearing primitive selection or viewport independence failed");
    return {};
}
[[nodiscard]] Result<void> finish_graphics_preparation(Engine& engine, Fixture& f)
{
    const GraphicsPreparationBudget budget{10, 4096, 32, 1};
    for (std::size_t i = 0; i < 1024; ++i) {
        const auto report = engine.advance_scene_graphics(*f.scene, budget);
        if (!report)
            return report.error();
        const auto& activity = report.value().activity;
        if (activity.work_units > 1 || activity.driver_calls > budget.driver_calls ||
            activity.uploaded_bytes > budget.upload_bytes)
            return failure("Public graphics preparation exceeded its service allowance");
        if (report.value().status.state == GraphicsPreparationState::ready)
            return {};
    }
    return failure("Public scene graphics preparation did not finish");
}
[[nodiscard]] Result<void> prepare_graphics(Engine& engine, Fixture& f)
{
    const auto idle = engine.scene_graphics_status(*f.scene);
    if (!idle || idle.value().state != GraphicsPreparationState::cpu_ready)
        return failure("An unprepared Scene had an incorrect public status");
    const auto positions = f.scene->document()->primitive_at(0).value().data.positions;
    const auto revision = f.scene->document()->revision();
    const auto begun = engine.advance_scene_graphics(*f.scene, {.work_units = 1});
    if (!begun || begun.value().status.state != GraphicsPreparationState::uploading)
        return failure("Public graphics preparation did not retain its admission");
    if (f.first->render(*f.scene, f.camera) || f.first->pick(*f.scene, f.camera, {48, 48}))
        return failure("An incomplete Scene reached public rendering or picking");
    if (const auto completed = finish_graphics_preparation(engine, f); !completed)
        return completed.error();
    if (f.scene->document()->revision() != revision ||
        f.scene->document()->primitive_at(0).value().data.positions.data() != positions.data())
        return failure("Public graphics preparation changed canonical source storage");
    return {};
}
[[nodiscard]] Result<void> verify_prepared_masks(Fixture& f)
{
    const auto document = *f.scene->document();
    const auto positions = document.primitive_at(0).value().data.positions;
    const auto source = ModelWriteSource::create(document).value();
    if (const auto checked = verify_masks(f); !checked)
        return checked.error();
    if (const auto checked = reject_invalid(f); !checked)
        return checked.error();
    if (const auto checked = reject_foreign(f); !checked)
        return checked.error();
    if (const auto checked = primitive_selection(f); !checked)
        return checked.error();
    if (const auto checked = clear_primitive_selection(f); !checked)
        return checked.error();
    if (!source.matches_document(*f.scene->document()) ||
        f.scene->document()->primitive_at(0).value().data.positions.data() != positions.data())
        return failure("View-local primitive masks mutated or copied canonical payloads");
    return {};
}
[[nodiscard]] Result<void> framed_pivot(Fixture& f, std::span<const EntityId> roots, float expected)
{
    if (const auto framed = f.first->fit_to_entities(*f.scene, f.camera, roots); !framed)
        return framed.error();
    const auto navigation = f.first->navigation_snapshot();
    if (!navigation || std::abs(navigation->pivot.x - expected) > .0001F)
        return failure("Entity framing ignored a selected branch or the resolved view masks");
    return {};
}
[[nodiscard]] Result<void> framing_rejection(Fixture& f)
{
    const auto camera = f.scene->local_matrix(f.camera).value();
    const std::array excluded{f.entities[0]};
    const std::array invalid{f.entities[1], EntityId{}};
    if (f.first->fit_to_entities(*f.scene, f.camera, excluded) ||
        f.first->fit_to_entities(*f.scene, f.camera, invalid) ||
        f.first->fit_to_entities(*f.scene, f.camera, {}) ||
        f.scene->local_matrix(f.camera).value().elements != camera.elements)
        return failure("Rejected entity framing changed the camera or accepted excluded roots");
    return {};
}
[[nodiscard]] Result<void> configure_entity_framing(Fixture& f)
{
    f.first->clear_isolation();
    f.first->clear_primitive_visibility();
    f.first->clear_clipping();
    const std::array hidden{PrimitiveInstance{f.entities[1], f.primitives[0]}};
    const std::array isolated{f.entities[1]};
    const std::array selected{PrimitiveInstance{f.entities[1], f.primitives[1]}};
    const SectionPlane plane{{10, 0, 0}, {-1, 0, 0}, PlaneHalfSpace::positive, true};
    if (!f.first->set_primitive_local_visibility(*f.scene, hidden, false) ||
        !f.first->isolate_entities(*f.scene, isolated) || !f.first->set_section_plane(plane) ||
        !f.first->set_selected_primitives(*f.scene, selected))
        return failure("Cannot configure the masked entity-framing case");
    return {};
}
[[nodiscard]] Result<void> verify_entity_framing(Fixture& f)
{
    const auto revision = f.scene->document()->revision();
    if (const auto configured = configure_entity_framing(f); !configured)
        return configured.error();
    const auto selection = f.first->selection_snapshot();
    if (const auto framed = framed_pivot(f, f.entities, 9.5F); !framed)
        return framed.error();
    if (const auto rejected = framing_rejection(f); !rejected)
        return rejected.error();
    const bool preserved = f.first->isolated_entity() == f.entities[1] &&
                           f.first->selection_snapshot() == selection &&
                           f.scene->document()->revision() == revision;
    if (!preserved)
        return failure("Entity framing mutated isolation, selection or the Document");
    f.first->clear_isolation();
    const std::array selected{PrimitiveInstance{f.entities[1], f.primitives[1]}};
    if (!f.first->isolate_primitives(*f.scene, selected))
        return failure("Cannot configure primitive-isolated entity framing");
    return framed_pivot(f, f.entities, 9.5F);
}
[[nodiscard]] Result<EntityId> procedural_branch(Scene& scene)
{
    const std::array<VertexPositionNormal, 3> vertices{
        {{{3, -1, 0}, {0, 0, 1}}, {{5, -1, 0}, {0, 0, 1}}, {{4, 1, 0}, {0, 0, 1}}}};
    constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
    const auto mesh = scene.create_mesh(MeshDataView{vertices, indices});
    const auto material = scene.create_material({});
    const auto root = scene.create_entity();
    if (!mesh || !material || !root)
        return failure("Cannot create a procedural framing branch");
    const auto model = scene.create_model_entity(mesh.value(), material.value());
    if (!model)
        return model.error();
    if (const auto parented = scene.set_parent(model.value(), root.value()); !parented)
        return parented.error();
    return root.value();
}
[[nodiscard]] Result<void> verify_procedural_framing(Engine& engine, Viewport& viewport,
                                                     EntityId foreign)
{
    auto made = engine.create_scene();
    if (!made)
        return made.error();
    auto& scene = *made.value();
    const auto root = procedural_branch(scene);
    const auto camera = scene.create_perspective_camera_entity({});
    if (!root || !camera)
        return failure("Cannot create the procedural framing scene");
    const std::array roots{root.value(), root.value()};
    if (scene.document() || !viewport.fit_to_entities(scene, camera.value(), roots))
        return failure("Entity framing required a Document or rejected duplicate branch roots");
    const auto navigation = viewport.navigation_snapshot();
    if (!navigation || std::abs(navigation->pivot.x - 4) > .0001F)
        return failure("Entity framing omitted a procedural branch's child geometry");
    const auto before = scene.local_matrix(camera.value()).value();
    const std::array rejected{foreign};
    if (viewport.fit_to_entities(scene, camera.value(), rejected) ||
        scene.local_matrix(camera.value()).value().elements != before.elements)
        return failure("Foreign entity framing changed the procedural camera");
    return {};
}
[[nodiscard]] bool retired_storage_empty(const GraphicsRetirementStatus& status) noexcept
{
    return status.estimated_geometry_bytes == 0 && status.estimated_texture_bytes == 0 &&
           status.picking_bytes == 0 && status.staging_bytes == 0;
}
[[nodiscard]] Result<void> drain_graphics(Engine& engine)
{
    const GraphicsRetirementBudget budget{10, 1, 3, 1};
    for (std::size_t i = 0; i < 4096; ++i) {
        const auto report = engine.advance_graphics_retirement(budget);
        if (!report)
            return report.error();
        const auto& work = report.value().activity;
        if (work.work_units > 1 || work.driver_calls > 3 || work.deleted_objects > 1)
            return failure("Public retirement exceeded its single-unit allowance");
        const auto& status = report.value().status;
        if (status.empty()) {
            if (!retired_storage_empty(status))
                return failure("Public retirement retained model-owned storage estimates");
            return {};
        }
    }
    return failure("Public graphics retirement did not complete");
}
[[nodiscard]] Result<void> retire_fixture(Engine& engine, Fixture& f)
{
    auto retired = f.scene->retire_cpu_state();
    if (!retired || !retired.value().has_document())
        return failure("Public Scene CPU retirement did not transfer its Document");
    retired.value() = {}; // No canonical source remains during cache retirement.
    const auto pending = engine.graphics_retirement_status();
    if (!pending || pending.value().empty())
        return failure("Scene CPU retirement synchronously discarded all graphics owners");
    const auto zero = engine.advance_graphics_retirement({.milliseconds = 0});
    if (!zero || zero.value().activity.work_units != 0 || zero.value().status.empty())
        return failure("Zero-budget retirement advanced detached owners");
    return drain_graphics(engine);
}
[[nodiscard]] Result<Fixture> make_culling_fixture(Engine& engine)
{
    Document document;
    const auto scene = document.create_scene("Backface culling").value();
    const auto mesh = document.create_mesh("Back-facing quad").value();
    const auto material = document.create_material({}).value();
    auto data = quad(0);
    for (std::size_t i = 0; i < data.indices.size(); i += 3)
        std::swap(data.indices[i], data.indices[i + 1]);
    const auto primitive = document.create_primitive(mesh, material, std::move(data)).value();
    const auto node = document.create_node("Back face").value();
    if (!document.set_node_mesh(node, mesh) || !document.add_scene_root(scene, node))
        return failure("Cannot create the backface fixture");
    auto created = engine.create_scene_from_document(std::move(document), scene);
    if (!created)
        return created.error();
    Fixture f;
    f.scene = std::move(created).value();
    f.camera = f.scene->create_perspective_camera_entity({}).value();
    f.entities[0] = *f.scene->entity_for_document_node(node).value();
    f.primitives[0] = primitive;
    f.first = engine.create_viewport({96, 96}).value();
    f.second = engine.create_viewport({96, 96}).value();
    f.first->set_clear_color({0, 0, 0, 1});
    f.first->set_render_shading_mode(RenderShadingMode::unlit);
    if (!f.first->fit_to_scene(*f.scene, f.camera))
        return failure("Cannot frame the backface fixture");
    return f;
}
[[nodiscard]] Result<void> verify_backface_pixels(Fixture& f, bool visible)
{
    const auto image = highlight_image(f, false);
    if (!image)
        return image.error();
    const auto pixel = center_pixel(f, image.value(), 0);
    if (!pixel || (pixel.value()[0] > 100) != visible)
        return failure("Rendering did not apply the view-local backface override");
    const auto expected =
        visible ? std::optional{PrimitiveInstance{f.entities[0], f.primitives[0]}} : std::nullopt;
    return picked(f, 0, expected);
}
[[nodiscard]] Result<void> verify_default_culling(Fixture& f)
{
    if (!f.first->backface_culling_enabled() || !f.second->backface_culling_enabled())
        return failure("Backface culling was not enabled by default");
    if (const auto checked = verify_backface_pixels(f, false); !checked)
        return checked.error();
    const auto point = f.first->project_world_to_viewport(*f.scene, f.camera, {0, 0, 0}).value();
    const auto two_sided = f.first->pick(*f.scene, f.camera, point.position_pixels,
                                         {.respect_material_sidedness = false});
    if (!two_sided || !two_sided.value())
        return failure("GPU picking ignored explicit two-sided PickOptions");
    return {};
}
[[nodiscard]] Result<void> verify_culling_override(Fixture& f)
{
    const auto view_revision = f.first->render_revision();
    f.first->set_backface_culling_enabled(false);
    if (f.first->backface_culling_enabled() || !f.second->backface_culling_enabled() ||
        f.first->render_revision() != view_revision + 1)
        return failure("Culling did not invalidate exactly its own viewport");
    f.first->set_backface_culling_enabled(false);
    if (f.first->render_revision() != view_revision + 1)
        return failure("An unchanged culling setting invalidated retained rendering");
    return verify_backface_pixels(f, true);
}
[[nodiscard]] Result<void> verify_culling(Engine& engine)
{
    auto made = make_culling_fixture(engine);
    if (!made)
        return made.error();
    auto& f = made.value();
    if (const auto prepared = finish_graphics_preparation(engine, f); !prepared)
        return prepared.error();
    const auto revision = f.scene->document()->revision();
    if (const auto checked = verify_default_culling(f); !checked)
        return checked.error();
    if (const auto checked = verify_culling_override(f); !checked)
        return checked.error();
    f.first->set_backface_culling_enabled(true);
    if (const auto checked = verify_backface_pixels(f, false); !checked)
        return checked.error();
    if (f.scene->document()->revision() != revision ||
        f.scene->document()->material_at(0).value().description.double_sided)
        return failure("View culling changed the authored material or Document revision");
    return retire_fixture(engine, f);
}
[[nodiscard]] Result<void> run(Engine& engine)
{
    if (const auto checked = verify_culling(engine); !checked)
        return checked.error();
    auto made = make_fixture(engine);
    if (!made)
        return made.error();
    auto& f = made.value();
    if (const auto prepared = prepare_graphics(engine, f); !prepared)
        return prepared.error();
    if (const auto checked = verify_prepared_masks(f); !checked)
        return checked.error();
    if (const auto framed = verify_entity_framing(f); !framed)
        return framed.error();
    if (const auto framed = verify_procedural_framing(engine, *f.second, f.entities[0]); !framed)
        return framed.error();
    return retire_fixture(engine, f);
}
} // namespace
Result<void> verify_primitive_visibility_contract(Engine& engine) noexcept
{
    try {
        return run(engine);
    } catch (const std::bad_alloc&) {
        fatal_error("Primitive visibility test memory allocation failed");
    } catch (...) {
        fatal_error("Unexpected primitive visibility test exception");
    }
}
} // namespace elf3d::test
