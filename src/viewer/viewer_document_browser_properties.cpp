#include "viewer_components.hpp"
#include "viewer_document_browser_ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <limits>
#include <string>
#include <utility>

namespace elf3d::viewer {
namespace {
void reveal_button(ViewerFrameContext& frame, const SceneSession& scene, BrowserSelectionKey key,
                   const char* label)
{
    if (ImGui::Button(label)) {
        frame.shell.show_scene_hierarchy = true;
        request_browser_reveal(frame.hierarchy.browser, std::move(key), scene.scene->id());
    }
}

void node_properties(ViewerFrameContext& frame, const SceneSession& scene, DocumentView document,
                     NodeId id)
{
    const auto node = document.node(id);
    if (!node) {
        browser_text(node.error().message());
        return;
    }
    browser_text(node.value().name.empty() ? "Unnamed Node" : node.value().name);
    ImGui::Text("Node ID: %llu; children: %zu", static_cast<unsigned long long>(id.debug_value()),
                node.value().children.size());
    const auto active = scene.scene->entity_for_document_node(id);
    if (active && active.value()) {
        ImGui::Text("Active runtime instance: %llu",
                    static_cast<unsigned long long>(active.value()->debug_value()));
    } else {
        ImGui::TextUnformatted("Active runtime instance: none");
    }
    ImGui::Text("Camera component: %s", node.value().perspective_camera ? "Yes" : "No");
    if (node.value().mesh) {
        const auto mesh = document.mesh(*node.value().mesh).value();
        ImGui::TextUnformatted("Mesh:");
        browser_text(mesh.name.empty() ? "Unnamed Mesh" : mesh.name);
        reveal_button(frame, scene, mesh.id, "Reveal Mesh Resource");
    }
}

void draw_user(ViewerFrameContext& frame, const SceneSession& scene,
               const DocumentHierarchySnapshot& index, NodeId user)
{
    const auto node = index.node(user).value();
    const auto scope = std::to_string(user.debug_value());
    ImGui::PushID(scope.c_str());
    if (ImGui::Selectable("##user", false)) {
        frame.shell.show_scene_hierarchy = true;
        request_browser_reveal(frame.hierarchy.browser, user, scene.scene->id());
    }
    const auto name = node.name.empty() ? std::string_view{"Unnamed Node"} : node.name;
    ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(),
                              ImGui::GetItemRectMax(), ImGui::GetItemRectMax().x, name.data(),
                              browser_text_end(name), nullptr);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        browser_text(name);
        ImGui::TextUnformatted("Reveal this authored object in Objects.");
        ImGui::EndTooltip();
    }
    ImGui::PopID();
}

void mesh_users(ViewerFrameContext& frame, const SceneSession& scene, DocumentView document,
                MeshId mesh)
{
    const auto& state = frame.hierarchy.browser;
    if (!state.index.ready() || !state.index.matches_document(document)) {
        ImGui::TextDisabled("Instance information is loading.");
        return;
    }
    const auto users = state.index.mesh(mesh).value().users;
    ImGui::Text("Document users: %zu", users.size());
    if (state.objects_built) {
        const auto active = state.active_mesh_users.find(mesh);
        ImGui::Text("Active runtime instances: %llu",
                    static_cast<unsigned long long>(
                        active == state.active_mesh_users.end() ? 0 : active->second));
    }
    if (users.empty()) {
        ImGui::TextUnformatted("Unused resource. There is no spatial instance to frame or hide.");
        return;
    }
    if (users.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        ImGui::TextDisabled("Too many users to list.");
        return;
    }
    if (ImGui::BeginChild("##users", {0, 120}, ImGuiChildFlags_Borders)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(users.size()));
        while (clipper.Step()) {
            for (int position = clipper.DisplayStart; position < clipper.DisplayEnd; ++position) {
                draw_user(frame, scene, state.index, users[static_cast<std::size_t>(position)]);
            }
        }
    }
    ImGui::EndChild();
}

void mesh_properties(ViewerFrameContext& frame, const SceneSession& scene, DocumentView document,
                     MeshId id)
{
    const auto mesh = document.mesh(id);
    if (!mesh) {
        browser_text(mesh.error().message());
        return;
    }
    browser_text(mesh.value().name.empty() ? "Unnamed Mesh" : mesh.value().name);
    ImGui::Text("Mesh ID: %llu; primitives: %zu", static_cast<unsigned long long>(id.debug_value()),
                mesh.value().primitives.size());
    mesh_users(frame, scene, document, id);
}

void primitive_properties(ViewerFrameContext& frame, const SceneSession& scene,
                          DocumentView document, PrimitiveId id)
{
    const auto source = document.primitive(id);
    if (!source) {
        browser_text(source.error().message());
        return;
    }
    const auto& primitive = source.value();
    ImGui::Text("Primitive ID: %llu", static_cast<unsigned long long>(id.debug_value()));
    const auto mesh = document.mesh(primitive.mesh).value();
    ImGui::TextUnformatted("Mesh:");
    browser_text(mesh.name.empty() ? "Unnamed Mesh" : mesh.name);
    ImGui::Text("Vertices: %zu; triangles: %zu", primitive.data.positions.size(),
                primitive.data.indices.size() / 3);
    ImGui::Text("Material ID: %llu",
                static_cast<unsigned long long>(primitive.material.debug_value()));
    reveal_button(frame, scene, primitive.mesh, "Reveal Mesh Resource");
}
} // namespace

bool draw_browser_properties(ViewerFrameContext& frame, const SceneSession& scene,
                             SelectionTool& selection)
{
    const auto primary = selection.browser_primary();
    const auto document = scene.scene->document();
    if (!primary || !document || std::holds_alternative<EntityId>(*primary)) {
        return false;
    }
    const auto scope = std::to_string(scene.scene->id().debug_value());
    ImGui::PushID(scope.c_str());
    ImGui::Text("Selected %s: %zu",
                selection.browser_domain() == BrowserSelectionDomain::resources ? "resources"
                                                                                : "objects",
                selection.browser_selection().size());
    if (const auto* node = std::get_if<NodeId>(&*primary)) {
        node_properties(frame, scene, *document, *node);
    }
    if (const auto* mesh = std::get_if<MeshId>(&*primary)) {
        mesh_properties(frame, scene, *document, *mesh);
    }
    if (const auto* primitive = std::get_if<PrimitiveId>(&*primary)) {
        primitive_properties(frame, scene, *document, *primitive);
    }
    ImGui::PopID();
    return true;
}
} // namespace elf3d::viewer
