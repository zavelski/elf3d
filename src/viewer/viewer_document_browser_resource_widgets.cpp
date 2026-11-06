#include "viewer_components.hpp"
#include "viewer_document_browser_ui.hpp"
#include "viewer_viewport.hpp"

#include <imgui.h>

namespace elf3d::viewer {
namespace {
[[nodiscard]] bool is_resource(const BrowserRow& row)
{
    return row.kind == BrowserRowKind::mesh || row.kind == BrowserRowKind::primitive;
}
[[nodiscard]] bool has_instances(const DocumentBrowserState& state, const BrowserRow& row)
{
    const auto mesh = row.kind == BrowserRowKind::mesh
                          ? browser_row_id<MeshId>(row)
                          : state.index.primitive(browser_row_id<PrimitiveId>(row)).value().mesh;
    const auto found = state.active_mesh_users.find(mesh);
    return found != state.active_mesh_users.end() && found->second != 0;
}
void run_action(BrowserPanelContext& context, const BrowserRow& row, BrowserResourceAction action)
{
    context.commands.emit(ResourceActionCommand{*row.key, action});
}
void visibility_mark(BrowserResourceVisibility state)
{
    const auto minimum = ImGui::GetItemRectMin();
    const auto maximum = ImGui::GetItemRectMax();
    const ImVec2 a{minimum.x + 5, minimum.y + 5}, b{maximum.x - 5, maximum.y - 5};
    auto* draw = ImGui::GetWindowDrawList();
    const auto color = ImGui::GetColorU32(ImGuiCol_Text);
    draw->AddRect(a, b, color);
    if (state.effective == BrowserVisibility::mixed || state.local == BrowserVisibility::mixed)
        draw->AddRectFilled(a, {(a.x + b.x) / 2, b.y}, color);
    else if (state.effective == BrowserVisibility::visible)
        draw->AddRectFilled(a, b, color);
    else if (state.local == BrowserVisibility::visible)
        draw->AddLine(a, b, color);
}
} // namespace
void draw_resource_context(BrowserPanelContext& context, const BrowserRow& row)
{
    if (!is_resource(row))
        return;
    ImGui::Separator();
    ImGui::TextDisabled("Actions apply to the clicked resource.");
    const bool available =
        has_instances(context.frame.hierarchy.browser, row) &&
        viewer_command_enabled(ResourceActionCommand{*row.key, BrowserResourceAction::frame},
                               context.commands.capabilities());
    ImGui::BeginDisabled(!available);
    if (ImGui::MenuItem("Frame All Instances"))
        run_action(context, row, BrowserResourceAction::frame);
    if (ImGui::MenuItem("Hide All Instances in Current View"))
        run_action(context, row, BrowserResourceAction::hide);
    if (ImGui::MenuItem("Show All Instances in Current View"))
        run_action(context, row, BrowserResourceAction::show);
    if (ImGui::MenuItem("Isolate All Instances in Current View"))
        run_action(context, row, BrowserResourceAction::isolate);
    ImGui::EndDisabled();
    if (!available)
        ImGui::TextDisabled("No active scene instances.");
    ImGui::TextDisabled("Framing respects visibility and clipping.");
}
void draw_resource_visibility(BrowserPanelContext& context, const BrowserRow& row,
                              std::size_t row_index)
{
    if (!is_resource(row))
        return;
    const auto state = browser_resource_visibility(
        context.frame.hierarchy.browser, *context.scene.scene, context.viewport, row_index);
    ImGui::SameLine();
    const bool available =
        state.local != BrowserVisibility::unavailable &&
        viewer_command_enabled(ResourceActionCommand{*row.key, BrowserResourceAction::hide},
                               context.commands.capabilities());
    ImGui::BeginDisabled(!available);
    if (ImGui::Button("##visibility", {ImGui::GetFrameHeight(), ImGui::GetFrameHeight()}))
        run_action(context, row,
                   state.local == BrowserVisibility::visible ? BrowserResourceAction::hide
                                                             : BrowserResourceAction::show);
    visibility_mark(state);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(
            "Changes this resource's active instances in the current view.\n"
            "Half-filled: mixed. Diagonal: excluded by object visibility or isolation.\n"
            "Unused and inactive resources have no spatial instance.");
}
} // namespace elf3d::viewer
