#include "viewer_components.hpp"

#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <elf3d/core/assert.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elf3d::viewer {

[[nodiscard]] bool edit_float3(const char* label, elf3d::Float3& value, float speed, float minimum,
                               float maximum) {
    std::array<float, 3> components{value.x, value.y, value.z};
    if (!ImGui::DragFloat3(label, components.data(), speed, minimum, maximum, "%.4g")) {
        return false;
    }
    value = {components[0], components[1], components[2]};
    return true;
}

[[nodiscard]] bool draw_section_plane_presets(elf3d::SectionPlane& plane,
                                              const std::optional<elf3d::Bounds3>& visible_bounds) {
    bool changed = false;
    if (ImGui::SmallButton("X")) {
        plane.normal = {1.0F, 0.0F, 0.0F};
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Y")) {
        plane.normal = {0.0F, 1.0F, 0.0F};
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Z")) {
        plane.normal = {0.0F, 0.0F, 1.0F};
        changed = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!visible_bounds.has_value());
    if (ImGui::SmallButton("Center") && visible_bounds.has_value()) {
        plane.point = bounds_center(*visible_bounds);
        changed = true;
    }
    ImGui::EndDisabled();
    return changed;
}

void draw_section_plane_editor(ViewerFrameContext& state, elf3d::Viewport& viewport,
                               const elf3d::ClippingSnapshot& snapshot,
                               const std::optional<elf3d::Bounds3>& visible_bounds) {
    if (!ImGui::CollapsingHeader("Section Plane", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    elf3d::SectionPlane plane = snapshot.section_plane;
    bool changed = ImGui::Checkbox("Enabled##SectionPlane", &plane.enabled);
    changed |= edit_float3("Point", plane.point, 0.05F, -100000.0F, 100000.0F);
    changed |= edit_float3("Normal", plane.normal, 0.02F, -1.0F, 1.0F);
    int retained_side = plane.retained_half_space == elf3d::PlaneHalfSpace::positive ? 0 : 1;
    const char* side_names[] = {"Positive", "Negative"};
    if (ImGui::Combo("Retained side", &retained_side, side_names,
                     static_cast<int>(std::size(side_names)))) {
        plane.retained_half_space =
            retained_side == 0 ? elf3d::PlaneHalfSpace::positive : elf3d::PlaneHalfSpace::negative;
        changed = true;
    }
    changed |= draw_section_plane_presets(plane, visible_bounds);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        viewport.clear_section_plane();
        return;
    }
    if (changed) {
        const elf3d::Result<void> result = viewport.set_section_plane(plane);
        if (!result) {
            set_viewport_error(state, result.error());
        }
    }
}

struct ClippingBoxRow {
    std::uint32_t index = 0;
    elf3d::ClippingBox box;
    bool has_visible_bounds = false;
};

struct ClippingPanelContext {
    ViewerFrameContext& state;
    const SceneSession& scene;
    elf3d::Viewport& viewport;
    ClippingTool& tool;
};

[[nodiscard]] bool draw_clipping_box_row(ViewerFrameContext& state, const SceneSession& scene,
                                         elf3d::Viewport& viewport, ClippingTool& clipping_tool,
                                         ClippingBoxRow row) {
    ImGui::PushID(static_cast<int>(row.index));
    ImGui::Separator();
    ImGui::Text("Box %u", row.index + 1U);
    bool changed = ImGui::Checkbox("Enabled", &row.box.enabled);
    changed |= edit_float3("Minimum", row.box.minimum, 0.05F, -100000.0F, 100000.0F);
    changed |= edit_float3("Maximum", row.box.maximum, 0.05F, -100000.0F, 100000.0F);
    if (!valid_box_for_commit(row.box)) {
        ImGui::TextColored(ImVec4{1.0F, 0.45F, 0.25F, 1.0F},
                           "Box extents must be positive on all axes.");
    } else if (changed) {
        const elf3d::Result<void> result = viewport.set_clipping_box(row.index, row.box);
        if (!result) {
            set_viewport_error(state, result.error());
        }
    }
    ImGui::BeginDisabled(!row.has_visible_bounds);
    if (ImGui::SmallButton("Reset to Visible Bounds")) {
        const elf3d::Result<void> result =
            clipping_tool.reset_box_to_visible_bounds(*scene.scene, viewport, row.index);
        if (!result) {
            set_viewport_error(state, result.error());
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool remove = ImGui::SmallButton("Remove");
    if (remove) {
        const elf3d::Result<void> result = viewport.remove_clipping_box(row.index);
        if (!result) {
            set_viewport_error(state, result.error());
        }
    }
    ImGui::PopID();
    return remove;
}

void draw_clipping_box_collection(const ClippingPanelContext& context,
                                  const elf3d::ClippingSnapshot& snapshot,
                                  bool has_visible_bounds) {
    if (!ImGui::CollapsingHeader("Clipping Boxes", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    for (std::uint32_t index = 0; index < snapshot.box_count; ++index) {
        if (draw_clipping_box_row(
                context.state, context.scene, context.viewport, context.tool,
                ClippingBoxRow{index, snapshot.boxes[index], has_visible_bounds})) {
            break;
        }
    }
    ImGui::Separator();
    ImGui::BeginDisabled(snapshot.box_count >= elf3d::maximum_clipping_boxes ||
                         !has_visible_bounds);
    if (ImGui::Button("Add Box from Visible Bounds")) {
        const elf3d::Result<std::uint32_t> result =
            context.tool.add_box_from_visible_bounds(*context.scene.scene, context.viewport);
        if (!result) {
            set_viewport_error(context.state, result.error());
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(snapshot.box_count == 0);
    if (ImGui::Button("Clear Boxes")) {
        context.viewport.clear_clipping_boxes();
    }
    ImGui::EndDisabled();
}

[[nodiscard]] bool edit_helper_color(const char* label, elf3d::Color4& color) {
    std::array<float, 4> components{color.red, color.green, color.blue, color.alpha};
    if (!ImGui::ColorEdit4(label, components.data(), ImGuiColorEditFlags_NoInputs)) {
        return false;
    }
    color = {components[0], components[1], components[2], components[3]};
    return true;
}

void draw_clipping_helper_editor(ViewerFrameContext& state, ClippingTool& clipping_tool) {
    if (!ImGui::CollapsingHeader("Helpers", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ClippingToolSettings settings = clipping_tool.settings();
    bool changed = ImGui::Checkbox("Show helpers", &settings.helpers_visible);
    changed |= edit_helper_color("Plane color", settings.section_plane_color);
    changed |= edit_helper_color("Box color", settings.box_color);
    changed |= ImGui::DragFloat("Line thickness", &settings.line_thickness_pixels, 0.1F, 0.5F,
                                16.0F, "%.1f px");
    if (changed) {
        const elf3d::Result<void> result = clipping_tool.set_settings(settings);
        if (!result) {
            set_viewport_error(state, result.error());
        }
    }
}

void draw_clipping_footer(ViewerFrameContext& state, const SceneSession& scene,
                          elf3d::Viewport& viewport) {
    ImGui::Separator();
    if (ImGui::Button("Fit to Clipped Content")) {
        const elf3d::Result<void> result = viewport.fit_to_scene(*scene.scene, scene.camera);
        if (!result) {
            set_viewport_error(state, result.error());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Clipping")) {
        viewport.clear_clipping();
    }
}

void build_clipping_panel(ImGuiID dockspace_id, ViewerFrameContext& state,
                          const SceneSession& scene, elf3d::Viewport& engine_viewport,
                          ToolCoordinator& tools) {
    if (!state.shell.show_clipping_panel) {
        return;
    }
    set_default_dock(state.shell.dock_right_bottom_id != 0 ? state.shell.dock_right_bottom_id
                                                           : dockspace_id,
                     state.shell.apply_dock_layout);
    if (begin_panel_window("Clipping", &state.shell.show_clipping_panel,
                           state.presentation.panel_title_font)) {
        const ScopedFont panel_font{state.presentation.panel_content_font};
        const elf3d::ClippingSnapshot snapshot = engine_viewport.clipping_snapshot();
        const elf3d::Result<std::optional<elf3d::Bounds3>> bounds_result =
            engine_viewport.visible_bounds(*scene.scene);
        const elf3d::Result<std::optional<elf3d::Bounds3>> unclipped_bounds_result =
            engine_viewport.unclipped_visible_bounds(*scene.scene);
        const std::optional<elf3d::Bounds3> visible_bounds =
            bounds_result ? bounds_result.value() : std::nullopt;
        ImGui::TextUnformatted(clipping_status(snapshot, visible_bounds.has_value()).c_str());
        draw_section_plane_editor(state, engine_viewport, snapshot, visible_bounds);
        draw_clipping_box_collection(
            ClippingPanelContext{state, scene, engine_viewport, tools.clipping()}, snapshot,
            unclipped_bounds_result && unclipped_bounds_result.value().has_value());
        draw_clipping_helper_editor(state, tools.clipping());
        draw_clipping_footer(state, scene, engine_viewport);
    }
    ImGui::End();
}

void invalidate_hierarchy_snapshot(SceneSession& scene) noexcept {
    scene.hierarchy_snapshot_valid = false;
}

[[nodiscard]] bool refresh_hierarchy_snapshot(ViewerFrameContext& state, SceneSession& scene) {
    if (scene.hierarchy_snapshot_valid &&
        scene.hierarchy_snapshot.hierarchy_revision() == scene.scene->hierarchy_revision() &&
        scene.hierarchy_snapshot.visibility_revision() == scene.scene->visibility_revision()) {
        return true;
    }

    elf3d::Result<elf3d::SceneHierarchySnapshot> snapshot = scene.scene->hierarchy_snapshot();
    if (!snapshot) {
        set_viewport_error(state, snapshot.error());
        return false;
    }
    scene.hierarchy_snapshot = std::move(snapshot).value();
    scene.hierarchy_snapshot_valid = true;
    return true;
}

[[nodiscard]] std::vector<bool>
selected_hierarchy_ancestors(const std::vector<elf3d::SceneHierarchyItem>& items,
                             std::optional<elf3d::EntityId> selected,
                             std::optional<std::size_t>& selected_index) {
    std::vector<bool> ancestors(items.size(), false);
    selected_index.reset();
    if (!selected.has_value()) {
        return ancestors;
    }

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (items[index].entity == *selected) {
            selected_index = index;
            break;
        }
    }
    if (!selected_index.has_value()) {
        return ancestors;
    }

    const std::size_t selected_item_index = *selected_index;
    std::uint32_t wanted_depth = items[selected_item_index].depth;
    for (std::size_t index = selected_item_index; index > 0 && wanted_depth > 0;) {
        --index;
        if (items[index].depth + 1U == wanted_depth) {
            ancestors[index] = true;
            --wanted_depth;
        }
    }
    return ancestors;
}

void draw_hierarchy_visibility_commands(ViewerCommandDispatcher& commands,
                                        const elf3d::SceneHierarchyItem& item) {
    if (item.local_visible && ImGui::MenuItem("Hide")) {
        commands.emit(SetEntityVisibilityCommand{item.entity, false, EntityVisibilityScope::local});
    }
    if ((!item.local_visible || !item.effective_visible) && ImGui::MenuItem("Show")) {
        commands.emit(SetEntityVisibilityCommand{item.entity, true,
                                                 EntityVisibilityScope::entity_and_ancestors});
    }
}

void draw_hierarchy_isolation_commands(ViewerCommandDispatcher& commands,
                                       const elf3d::SceneHierarchyItem& item) {
    if (ImGui::MenuItem("Isolate")) {
        commands.emit(IsolateEntityCommand{item.entity});
    }
    if (commands.capabilities().isolating && ImGui::MenuItem("Exit Isolation")) {
        commands.emit(ExitIsolationCommand{});
    }
}

void build_hierarchy_row_context(ViewerCommandDispatcher& commands,
                                 const elf3d::SceneHierarchyItem& item) {
    const ScopedFont default_font{ImGui::GetDefaultFont()};
    if (!ImGui::BeginPopupContextItem()) {
        return;
    }
    if (ImGui::MenuItem("Select")) {
        commands.emit(SelectEntityCommand{item.entity});
    }
    draw_hierarchy_visibility_commands(commands, item);
    draw_hierarchy_isolation_commands(commands, item);
    ImGui::EndPopup();
}

void draw_hierarchy_summary(const SceneSession& scene, elf3d::Viewport& viewport,
                            ViewerCommandDispatcher& commands) {
    const elf3d::SceneHierarchyStatistics hierarchy = scene.scene->hierarchy_statistics();
    ImGui::Text("Entities: %llu  Roots: %llu  Hidden: %llu / %llu",
                static_cast<unsigned long long>(hierarchy.entities),
                static_cast<unsigned long long>(hierarchy.root_entities),
                static_cast<unsigned long long>(hierarchy.effectively_hidden_entities),
                static_cast<unsigned long long>(hierarchy.entities));
    const std::optional<elf3d::EntityId> isolated = viewport.isolated_entity();
    if (!isolated.has_value()) {
        ImGui::TextUnformatted("Isolation: none");
        return;
    }
    const std::string label = entity_label(scene, *isolated);
    ImGui::Text("Isolation: %s", label.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Exit Isolation")) {
        commands.emit(ExitIsolationCommand{});
    }
}

void draw_hierarchy_selection_actions(elf3d::Viewport& viewport,
                                      ViewerCommandDispatcher& commands) {
    const std::optional<EntityId> selected = viewport.selected_entity();
    ImGui::BeginDisabled(!selected.has_value());
    if (ImGui::SmallButton("Hide Selected") && selected.has_value()) {
        commands.emit(SetEntityVisibilityCommand{*selected, false, EntityVisibilityScope::local});
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Show Selected") && selected.has_value()) {
        commands.emit(SetEntityVisibilityCommand{*selected, true,
                                                 EntityVisibilityScope::entity_and_ancestors});
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Isolate Selected") && selected.has_value()) {
        commands.emit(IsolateEntityCommand{*selected});
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton("Show All")) {
        commands.emit(ShowAllEntitiesCommand{});
    }
}

struct HierarchyRows {
    std::vector<elf3d::SceneHierarchyItem> items;
    std::vector<std::string> names;
};

[[nodiscard]] HierarchyRows collect_hierarchy_rows(const SceneSession& scene) {
    HierarchyRows rows;
    rows.items.reserve(scene.hierarchy_snapshot.size());
    rows.names.reserve(scene.hierarchy_snapshot.size());
    for (std::size_t index = 0; index < scene.hierarchy_snapshot.size(); ++index) {
        const elf3d::Result<elf3d::SceneHierarchyItem> item =
            scene.hierarchy_snapshot.item_at(index);
        const elf3d::Result<std::string_view> name = scene.hierarchy_snapshot.name_at(index);
        if (item && name) {
            rows.items.push_back(item.value());
            rows.names.emplace_back(name.value());
        }
    }
    return rows;
}

[[nodiscard]] std::string hierarchy_row_label(const SceneSession& scene,
                                              const elf3d::SceneHierarchyItem& item,
                                              const std::string& source_name) {
    std::string label = source_name.empty() ? entity_label(scene, item.entity) : source_name;
    if (item.has_camera) {
        label += " [camera]";
    } else if (item.renderable) {
        label += " [model]";
    }
    return label;
}

[[nodiscard]] ImGuiTreeNodeFlags
hierarchy_row_flags(const elf3d::SceneHierarchyItem& item,
                    std::optional<elf3d::EntityId> selected) noexcept {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (item.child_count == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (selected == item.entity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    return flags;
}

void draw_hierarchy_visibility_state(const elf3d::SceneHierarchyItem& item,
                                     std::optional<elf3d::EntityId> isolated,
                                     ViewerCommandDispatcher& commands) {
    ImGui::SameLine();
    if (ImGui::SmallButton(item.local_visible ? "Hide##visible" : "Show##visible")) {
        commands.emit(SetEntityVisibilityCommand{item.entity, !item.local_visible,
                                                 EntityVisibilityScope::local});
    }
    if (!item.local_visible) {
        ImGui::SameLine();
        ImGui::TextDisabled("local hidden");
    } else if (!item.effective_visible) {
        ImGui::SameLine();
        ImGui::TextDisabled("inherited hidden");
    }
    if (isolated == item.entity) {
        ImGui::SameLine();
        ImGui::TextUnformatted("isolated");
    }
}

struct HierarchyTreeContext {
    std::optional<elf3d::EntityId> selected;
    std::optional<elf3d::EntityId> isolated;
    std::optional<std::size_t> selected_index;
    std::vector<bool> ancestors;
    bool should_reveal = false;
};

struct HierarchyRowContext {
    const HierarchyRows* rows = nullptr;
    const HierarchyTreeContext* tree = nullptr;
    ViewerCommandDispatcher* commands = nullptr;
    std::size_t index = 0;
};

[[nodiscard]] bool draw_hierarchy_tree_row(ViewerFrameContext& state, SceneSession& scene,
                                           const HierarchyRowContext& row_context) {
    ELF3D_ASSERT(row_context.rows != nullptr);
    ELF3D_ASSERT(row_context.tree != nullptr);
    ELF3D_ASSERT(row_context.commands != nullptr);
    const HierarchyRows& rows = *row_context.rows;
    const HierarchyTreeContext& context = *row_context.tree;
    const std::size_t index = row_context.index;
    const elf3d::SceneHierarchyItem& item = rows.items[index];
    if (context.ancestors[index]) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    const std::string label = hierarchy_row_label(scene, item, rows.names[index]);
    const std::string id = std::to_string(item.entity.debug_value());
    ImGui::PushID(id.c_str());
    const bool open = ImGui::TreeNodeEx("##entity", hierarchy_row_flags(item, context.selected),
                                        "%s", label.c_str());
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        row_context.commands->emit(SelectEntityCommand{item.entity});
    }
    if (context.should_reveal && context.selected_index == index) {
        ImGui::SetScrollHereY(0.5F);
        state.hierarchy.last_revealed_selection = item.entity;
    }
    build_hierarchy_row_context(*row_context.commands, item);
    draw_hierarchy_visibility_state(item, context.isolated, *row_context.commands);
    ImGui::PopID();
    return open;
}

[[nodiscard]] std::size_t next_hierarchy_row(const HierarchyRows& rows, std::size_t index,
                                             bool open) noexcept {
    const elf3d::SceneHierarchyItem& item = rows.items[index];
    ++index;
    if (item.child_count == 0 || open) {
        return index;
    }
    while (index < rows.items.size() && rows.items[index].depth > item.depth) {
        ++index;
    }
    return index;
}

void draw_hierarchy_tree(ViewerFrameContext& state, SceneSession& scene, elf3d::Viewport& viewport,
                         const HierarchyRows& rows, ViewerCommandDispatcher& commands) {
    HierarchyTreeContext context;
    context.selected = viewport.selected_entity();
    context.isolated = viewport.isolated_entity();
    context.ancestors =
        selected_hierarchy_ancestors(rows.items, context.selected, context.selected_index);
    context.should_reveal =
        context.selected.has_value() && state.hierarchy.last_revealed_selection != context.selected;
    if (!context.selected.has_value()) {
        state.hierarchy.last_revealed_selection.reset();
    }
    ImGui::Separator();
    int open_depth = 0;
    for (std::size_t index = 0; index < rows.items.size();) {
        const elf3d::SceneHierarchyItem& item = rows.items[index];
        while (open_depth > static_cast<int>(item.depth)) {
            ImGui::TreePop();
            --open_depth;
        }
        const bool open = draw_hierarchy_tree_row(
            state, scene, HierarchyRowContext{&rows, &context, &commands, index});
        if (item.child_count != 0 && open) {
            ++open_depth;
        }
        index = next_hierarchy_row(rows, index, open);
    }
    while (open_depth > 0) {
        ImGui::TreePop();
        --open_depth;
    }
}

void build_scene_hierarchy_panel(ImGuiID dockspace_id, ViewerFrameContext& state,
                                 SceneSession& scene, elf3d::Viewport& engine_viewport,
                                 ViewerCommandDispatcher& commands) {
    if (!state.shell.show_scene_hierarchy) {
        return;
    }
    set_default_dock(state.shell.dock_right_id != 0 ? state.shell.dock_right_id : dockspace_id,
                     state.shell.apply_dock_layout);
    const bool open = begin_panel_window("Scene Hierarchy", &state.shell.show_scene_hierarchy,
                                         state.presentation.panel_title_font);
    if (!open || !refresh_hierarchy_snapshot(state, scene)) {
        if (open) {
            const ScopedFont panel_font{state.presentation.panel_content_font};
            ImGui::TextWrapped("Hierarchy unavailable: %s",
                               state.notifications.viewport_error.c_str());
        }
        ImGui::End();
        return;
    }
    const ScopedFont panel_font{state.presentation.panel_content_font};
    draw_hierarchy_summary(scene, engine_viewport, commands);
    draw_hierarchy_selection_actions(engine_viewport, commands);
    const HierarchyRows rows = collect_hierarchy_rows(scene);
    draw_hierarchy_tree(state, scene, engine_viewport, rows, commands);
    ImGui::End();
}

} // namespace elf3d::viewer
