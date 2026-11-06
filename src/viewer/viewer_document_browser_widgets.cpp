#include "viewer_components.hpp"
#include "viewer_document_browser_ui.hpp"
#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace elf3d::viewer {
const char* browser_text_end(std::string_view text) noexcept
{
    // Immediate ImGui pointer-pair boundary; no NUL-termination assumption.
    return text.empty() ? text.data() : text.data() + text.size();
}
void browser_text(std::string_view text)
{
    if (text.empty()) {
        ImGui::TextUnformatted("");
        return;
    }
    ImGui::TextUnformatted(text.data(), browser_text_end(text));
}
namespace {
enum class Glyph { scene, node, mesh, primitive, camera, light };

[[nodiscard]] Glyph node_glyph(const DocumentHierarchyNode& node)
{
    if (node.has_camera) {
        return Glyph::camera;
    }
    if (node.has_areal_light) {
        return Glyph::light;
    }
    return node.mesh ? Glyph::mesh : Glyph::node;
}

[[nodiscard]] Glyph row_glyph(const BrowserRow& row, const DocumentBrowserState& state,
                              const Scene& scene)
{
    if (row.kind == BrowserRowKind::scene) {
        return Glyph::scene;
    }
    if (row.kind == BrowserRowKind::mesh) {
        return Glyph::mesh;
    }
    if (row.kind == BrowserRowKind::primitive) {
        return Glyph::primitive;
    }
    if (row.kind == BrowserRowKind::runtime) {
        const auto info = scene.entity_info(browser_row_id<EntityId>(row));
        return info && info.value().has_camera ? Glyph::camera : Glyph::node;
    }
    if (row.kind == BrowserRowKind::node) {
        return node_glyph(state.index.node(browser_row_id<NodeId>(row)).value());
    }
    return Glyph::node;
}

void draw_glyph(Glyph glyph, ImVec2 minimum, float size)
{
    auto* draw = ImGui::GetWindowDrawList();
    const auto color = ImGui::GetColorU32(ImGuiCol_Text);
    const float x = minimum.x + 2;
    const float y = minimum.y + 3;
    const float edge = size - 5;
    switch (glyph) {
    case Glyph::scene:
        draw->AddRect({x, y}, {x + edge, y + edge}, color);
        draw->AddLine({x, y + edge / 2}, {x + edge, y + edge / 2}, color);
        draw->AddLine({x + edge / 2, y}, {x + edge / 2, y + edge}, color);
        break;
    case Glyph::node:
        draw->AddRect({x, y + 3}, {x + edge, y + edge}, color);
        draw->AddLine({x, y}, {x + edge / 2, y}, color, 2);
        break;
    case Glyph::mesh:
        draw->AddRect({x, y}, {x + edge, y + edge}, color);
        draw->AddLine({x, y}, {x + edge, y + edge}, color);
        draw->AddLine({x + edge, y}, {x, y + edge}, color);
        break;
    case Glyph::primitive:
        draw->AddTriangle({x + edge / 2, y}, {x + edge, y + edge}, {x, y + edge}, color, 1.5F);
        break;
    case Glyph::camera:
        draw->AddRect({x, y + 2}, {x + edge * 0.7F, y + edge - 2}, color);
        draw->AddTriangle({x + edge * 0.7F, y + edge / 2}, {x + edge, y}, {x + edge, y + edge},
                          color);
        break;
    case Glyph::light:
        draw->AddCircle({x + edge / 2, y + edge / 2}, edge / 3, color, 8);
        draw->AddLine({x + edge / 2, y - 2}, {x + edge / 2, y + edge + 2}, color);
        break;
    }
}

[[nodiscard]] std::string fallback_name(const BrowserRow& row)
{
    if (row.kind == BrowserRowKind::node) {
        return "Unnamed Node";
    }
    if (row.kind == BrowserRowKind::runtime) {
        return "Runtime Object";
    }
    return std::string{browser_row_type(row)} + " " + std::to_string(row.source_index + 1);
}

void name_and_mesh_suffix(const DocumentBrowserState& state, const BrowserRow& row,
                          const Scene& scene, ImVec2 minimum, ImVec2 maximum)
{
    const auto name = browser_row_name(state, row, scene);
    const auto fallback = name.empty() ? fallback_name(row) : std::string{};
    const std::string_view label = name.empty() ? std::string_view{fallback} : name;
    minimum.y += (maximum.y - minimum.y - ImGui::GetFontSize()) * 0.5F;
    ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), minimum, maximum, maximum.x, label.data(),
                              browser_text_end(label), nullptr);
    if (row.kind != BrowserRowKind::node) {
        return;
    }
    const auto node = state.index.node(browser_row_id<NodeId>(row)).value();
    if (!node.mesh) {
        return;
    }
    const auto mesh_name = state.index.mesh(*node.mesh).value().name;
    const float end = minimum.x + ImGui::CalcTextSize(label.data(), browser_text_end(label)).x + 8;
    if (!mesh_name.empty() && mesh_name != name && maximum.x - end > 70) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), {end, minimum.y}, maximum, maximum.x,
                                  mesh_name.data(), browser_text_end(mesh_name), nullptr);
        ImGui::PopStyleColor();
    }
}

[[nodiscard]] std::optional<EntityId> row_entity(const BrowserRow& row, const Scene& scene)
{
    if (!row.key) {
        return {};
    }
    if (const auto* entity = std::get_if<EntityId>(&*row.key)) {
        return *entity;
    }
    if (const auto* node = std::get_if<NodeId>(&*row.key)) {
        const auto entity = scene.entity_for_document_node(*node);
        return entity ? entity.value() : std::optional<EntityId>{};
    }
    return {};
}

void visibility_glyph(const EntityInfo& info, bool mixed)
{
    const auto minimum = ImGui::GetItemRectMin();
    const auto maximum = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    const auto color = ImGui::GetColorU32(ImGuiCol_Text);
    const ImVec2 a{minimum.x + 5, minimum.y + 5}, b{maximum.x - 5, maximum.y - 5};
    draw->AddRect(a, b, color);
    if (mixed) {
        draw->AddRectFilled(a, {(a.x + b.x) / 2, b.y}, color);
    } else if (info.effective_visible) {
        draw->AddRectFilled(a, b, color);
    } else if (info.local_visible) {
        draw->AddLine(a, b, color);
    }
}

void visibility_control(BrowserPanelContext& context, const BrowserRow& row, std::size_t row_index)
{
    const auto entity = row_entity(row, *context.scene.scene);
    if (!entity) {
        draw_resource_visibility(context, row, row_index);
        return;
    }
    const auto info = context.scene.scene->entity_info(*entity);
    if (!info) {
        return;
    }
    const bool mixed =
        browser_subtree_visibility(context.frame.hierarchy.browser, *context.scene.scene,
                                   row_index) == BrowserVisibility::mixed;
    ImGui::SameLine();
    if (ImGui::Button("##visibility", {ImGui::GetFrameHeight(), ImGui::GetFrameHeight()})) {
        context.commands.emit(SetEntityVisibilityCommand{*entity, !info.value().local_visible,
                                                         EntityVisibilityScope::local});
    }
    visibility_glyph(info.value(), mixed);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "%s\n%s\nChanges local instance visibility; Document data is unchanged.",
            !info.value().local_visible
                ? "Locally Hidden"
                : (info.value().effective_visible ? "Visible" : "Hidden by an Ancestor"),
            mixed ? "Active subtree has mixed visibility."
                  : "Effective Scene visibility; isolation is separate.");
    }
}

void spatial_context(BrowserPanelContext& context, EntityId entity)
{
    if (ImGui::MenuItem("Hide This Instance")) {
        context.commands.emit(
            SetEntityVisibilityCommand{entity, false, EntityVisibilityScope::local});
    }
    if (ImGui::MenuItem("Show This Instance and Ancestors")) {
        context.commands.emit(
            SetEntityVisibilityCommand{entity, true, EntityVisibilityScope::entity_and_ancestors});
    }
    if (ImGui::MenuItem("Isolate This Instance")) {
        context.commands.emit(IsolateEntityCommand{entity});
    }
}

[[nodiscard]] std::optional<MeshId> linked_mesh(const DocumentBrowserState& state,
                                                const BrowserRow& row)
{
    if (!row.key) {
        return {};
    }
    if (const auto* node = std::get_if<NodeId>(&*row.key)) {
        return state.index.node(*node).value().mesh;
    }
    if (const auto* primitive = std::get_if<PrimitiveId>(&*row.key)) {
        return state.index.primitive(*primitive).value().mesh;
    }
    if (const auto* mesh = std::get_if<MeshId>(&*row.key)) {
        return *mesh;
    }
    return {};
}

void resource_marker(const DocumentBrowserState& state, const BrowserRow& row, ImVec2 minimum,
                     ImVec2 maximum)
{
    const auto id = linked_mesh(state, row);
    if (!id) {
        return;
    }
    const auto users = state.index.mesh(*id).value().users.size();
    if (users == 1) {
        return;
    }
    auto* draw = ImGui::GetWindowDrawList();
    const float size = ImGui::GetFontSize() * 0.35F;
    const ImVec2 center{maximum.x - size - 2, (minimum.y + maximum.y) / 2};
    const auto color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    if (users == 0) {
        draw->AddCircle(center, size, color, 8);
    } else {
        draw->AddRect({center.x - size, center.y - size},
                      {center.x + size / 2, center.y + size / 2}, color);
        draw->AddRect({center.x - size / 2, center.y - size / 2},
                      {center.x + size, center.y + size}, color);
    }
}

void row_context_menu(BrowserPanelContext& context, const BrowserRow& row)
{
    if (!row.key || !ImGui::BeginPopupContextItem("##context")) {
        return;
    }
    auto& state = context.frame.hierarchy.browser;
    if (ImGui::MenuItem("Properties")) {
        context.frame.shell.show_selection_panel = true;
        const std::array<BrowserSelectionKey, 1> clicked{*row.key};
        if (const auto selected = context.tools.selection().set_browser_selection(
                *context.scene.scene, context.viewport, clicked);
            !selected) {
            set_viewport_error(context.frame, selected.error());
        }
    }
    if (const auto entity = row_entity(row, *context.scene.scene)) {
        spatial_context(context, *entity);
    }
    draw_resource_context(context, row);
    const auto mesh = linked_mesh(state, row);
    if (mesh && ImGui::MenuItem("Reveal Mesh Resource")) {
        request_browser_reveal(state, *mesh, context.scene.scene->id());
    }
    ImGui::EndPopup();
}

void row_tooltip(const DocumentBrowserState& state, const BrowserRow& row, const Scene& scene)
{
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        return;
    }
    ImGui::BeginTooltip();
    const auto name = browser_row_name(state, row, scene);
    const auto fallback = name.empty() ? fallback_name(row) : std::string{};
    browser_text(name.empty() ? std::string_view{fallback} : name);
    ImGui::TextDisabled("%s", browser_row_type(row));
    if (row.kind == BrowserRowKind::mesh) {
        const auto mesh = state.index.mesh(browser_row_id<MeshId>(row)).value();
        ImGui::Text("Document users: %zu; primitives: %zu", mesh.users.size(),
                    mesh.primitives.size());
        if (mesh.users.empty()) {
            ImGui::TextUnformatted("Unused resource; no object instance is invented.");
        } else if (mesh.users.size() > 1) {
            ImGui::TextUnformatted("Shared mesh resource.");
        }
    }
    ImGui::EndTooltip();
}
[[nodiscard]] std::array<char, 96> row_scope(const BrowserRow& row,
                                             const DocumentBrowserState& state)
{
    std::uint64_t identity = row.source_index;
    if (row.kind == BrowserRowKind::scene) {
        identity = state.index.scene_at(row.source_index).value().id.debug_value();
    }
    if (row.key) {
        identity = std::visit([](auto value) { return value.debug_value(); }, *row.key);
    }
    std::array<char, 96> id{};
    std::snprintf(id.data(), id.size(), "%u/%llu/%llu", static_cast<unsigned>(row.kind),
                  static_cast<unsigned long long>(identity),
                  static_cast<unsigned long long>(row.occurrence));
    return id;
}

void disclosure_and_glyph(BrowserPanelContext& context, BrowserProjection& projection,
                          BrowserRow& row, float height)
{
    if (row.expandable) {
        if (ImGui::ArrowButton("##expand", row.expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
            projection.bulk_expansion.reset();
            row.expanded = !row.expanded;
            projection.dirty = true;
        }
    } else {
        ImGui::Dummy({height, height});
    }
    ImGui::SameLine();
    const auto glyph_position = ImGui::GetCursorScreenPos();
    draw_glyph(row_glyph(row, context.frame.hierarchy.browser, *context.scene.scene),
               glyph_position, height);
    ImGui::Dummy({height, height});
    ImGui::SameLine();
}

void focus_revealed(DocumentBrowserState& state, const BrowserRow& row, std::size_t row_index)
{
    if (state.scroll_row == row_index && state.scroll_to == row.key && row.key) {
        ImGui::SetScrollHereY(0.5F);
        ImGui::SetKeyboardFocusHere(-1);
        state.scroll_to.reset();
        state.scroll_row.reset();
    }
}

void keyboard_expansion(DocumentBrowserState& state, BrowserProjection& projection,
                        std::size_t row_index)
{
    if (!ImGui::IsItemFocused()) {
        return;
    }
    auto& row = projection.rows[row_index];
    if (row.expandable && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        row.expanded = true;
        projection.dirty = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        if (row.expandable && row.expanded) {
            row.expanded = false;
            projection.dirty = true;
        } else if (row.parent && projection.rows[*row.parent].key) {
            // Parent already belongs to this projection; preserve search and
            // selection scope while moving keyboard focus within the list.
            state.scroll_to = projection.rows[*row.parent].key;
            state.scroll_row = row.parent;
        }
    }
    if (projection.dirty) {
        projection.bulk_expansion.reset();
    }
}

void draw_name(BrowserPanelContext& context, BrowserProjection& projection, std::size_t row_index,
               float height)
{
    const auto& row = projection.rows[row_index];
    auto& state = context.frame.hierarchy.browser;
    if (row.key) {
        ImGui::SetNextItemSelectionUserData(static_cast<ImGuiSelectionUserData>(row_index));
    }
    const bool selected = row.key && context.tools.selection().browser_selected(*row.key);
    const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x - height - 4);
    ImGui::Selectable("##row", selected,
                      row.key ? ImGuiSelectableFlags_None : ImGuiSelectableFlags_Disabled,
                      {width, height});
    focus_revealed(state, row, row_index);
    keyboard_expansion(state, projection, row_index);
    const auto minimum = ImGui::GetItemRectMin();
    const auto maximum = ImGui::GetItemRectMax();
    name_and_mesh_suffix(state, row, *context.scene.scene, minimum,
                         {maximum.x - ImGui::GetFontSize(), maximum.y});
    resource_marker(state, row, minimum, maximum);
    row_context_menu(context, row);
    row_tooltip(state, row, *context.scene.scene);
    visibility_control(context, row, row_index);
}
} // namespace

void draw_browser_row(BrowserPanelContext& context, BrowserProjection& projection,
                      std::size_t row_index, float height)
{
    auto& row = projection.rows[row_index];
    const auto id = row_scope(row, context.frame.hierarchy.browser);
    ImGui::PushID(id.data());
    const float indent = static_cast<float>(row.depth) * ImGui::GetFontSize() * 0.65F;
    ImGui::Indent(indent);
    disclosure_and_glyph(context, projection, row, height);
    draw_name(context, projection, row_index, height);
    ImGui::Unindent(indent);
    ImGui::PopID();
}
} // namespace elf3d::viewer
