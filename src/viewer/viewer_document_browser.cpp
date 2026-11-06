#include "viewer_components.hpp"
#include "viewer_document_browser_ui.hpp"
#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <string>

namespace elf3d::viewer {
namespace {
[[nodiscard]] bool key_less(const BrowserSelectionKey& a, const BrowserSelectionKey& b)
{
    if (a.index() != b.index()) {
        return a.index() < b.index();
    }
    const auto ordinal = [](auto value) { return value.debug_value(); };
    return std::visit(ordinal, a) < std::visit(ordinal, b);
}

[[nodiscard]] std::optional<std::size_t> visible_position(const BrowserProjection& projection,
                                                          ImGuiSelectionUserData item)
{
    if (item < 0) {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(item); // nonnegative; row count is int-bounded
    const auto found =
        std::lower_bound(projection.visible.begin(), projection.visible.end(), index);
    if (found == projection.visible.end() || *found != index) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - projection.visible.begin());
}

[[nodiscard]] std::vector<BrowserSelectionKey> range_keys(const BrowserProjection& projection,
                                                          const ImGuiSelectionRequest& request)
{
    const auto first = visible_position(projection, request.RangeFirstItem);
    const auto last = visible_position(projection, request.RangeLastItem);
    std::vector<BrowserSelectionKey> result;
    if (!first || !last) {
        return result;
    }
    const auto begin = std::min(*first, *last);
    const auto end = std::max(*first, *last);
    result.reserve(end - begin + 1);
    for (auto index = begin; index <= end; ++index) {
        const auto& row = projection.rows[projection.visible[index]];
        if (row.key) {
            result.push_back(*row.key);
        }
    }
    return result;
}

void apply_request(std::vector<BrowserSelectionKey>& values, const BrowserProjection& projection,
                   const ImGuiSelectionRequest& request)
{
    if (request.Type == ImGuiSelectionRequestType_SetAll) {
        values.clear();
        if (request.Selected) {
            for (const auto index : projection.visible) {
                if (projection.rows[index].key) {
                    values.push_back(*projection.rows[index].key);
                }
            }
        }
        return;
    }
    if (request.Type != ImGuiSelectionRequestType_SetRange) {
        return;
    }
    auto range = range_keys(projection, request);
    if (request.Selected) {
        values.insert(values.end(), range.begin(), range.end());
    } else {
        std::sort(range.begin(), range.end(), key_less);
        std::erase_if(values, [&](const auto& value) {
            return std::binary_search(range.begin(), range.end(), value, key_less);
        });
    }
}

void apply_requests(BrowserPanelContext& context, const BrowserProjection& projection,
                    const ImGuiMultiSelectIO& io, bool resources)
{
    if (io.Requests.empty()) {
        return;
    }
    auto& selection = context.tools.selection();
    std::vector<BrowserSelectionKey> values;
    const bool selected_resources = selection.browser_domain() == BrowserSelectionDomain::resources;
    if (selected_resources == resources) {
        const auto current = selection.browser_selection();
        values.assign(current.begin(), current.end());
    }
    for (const auto& request : io.Requests) {
        apply_request(values, projection, request);
    }
    if (const auto changed =
            selection.set_browser_selection(*context.scene.scene, context.viewport, values);
        !changed) {
        set_viewport_error(context.frame, changed.error());
    }
}

void projection_controls(BrowserProjection& projection)
{
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Search name or type", projection.search.data(),
                                 projection.search.size())) {
        projection.dirty = true;
    }
    constexpr std::array<const char*, 5> filters{"All Types", "Shared Meshes", "Unused Meshes",
                                                 "Cameras", "Lights"};
    int filter = static_cast<int>(projection.filter);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##filter", &filter, filters.data(), 5)) {
        projection.filter = static_cast<BrowserFilter>(filter); // closed Combo range [0,4]
        projection.dirty = true;
    }
    if (ImGui::SmallButton("Expand All")) {
        projection.bulk_expansion = true;
        projection.bulk_cursor = 0;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Collapse All")) {
        projection.bulk_expansion = false;
        projection.bulk_cursor = 0;
    }
}

void show_projection(BrowserPanelContext& context, BrowserProjection& projection, bool resources,
                     bool built)
{
    ImGui::PushID(resources ? "resources" : "objects");
    projection_controls(projection);
    if (!built || !projection.ready) {
        ImGui::TextDisabled("Preparing browser rows...");
    } else {
        if (projection.phase != BrowserProjection::Phase::complete) {
            ImGui::TextDisabled("Updating list...");
        }
        if (resources) {
            ImGui::TextWrapped("Resource actions affect active instances in this view.");
        }
        draw_browser_rows(context, projection, resources);
    }
    ImGui::PopID();
}

[[nodiscard]] bool locate_reveal(DocumentBrowserState& state, BrowserProjection& projection)
{
    for (std::size_t unit = 0; unit < 128 && state.reveal_cursor < projection.rows.size(); ++unit) {
        const auto index = state.reveal_cursor++;
        if (projection.rows[index].key == state.reveal) {
            state.reveal_row = index;
            state.reveal_parent = projection.rows[index].parent;
            return true;
        }
    }
    if (state.reveal_cursor == projection.rows.size()) {
        state.reveal.reset();
    }
    return false;
}

[[nodiscard]] bool open_reveal_parents(DocumentBrowserState& state, BrowserProjection& projection)
{
    for (std::size_t unit = 0; unit < 128 && state.reveal_parent; ++unit) {
        auto& parent = projection.rows[*state.reveal_parent];
        parent.expanded = true;
        state.reveal_parent = parent.parent;
    }
    return !state.reveal_parent;
}

void commit_reveal(BrowserPanelContext& context, BrowserProjection& projection, bool resources)
{
    auto& state = context.frame.hierarchy.browser;
    const std::array<BrowserSelectionKey, 1> values{*state.reveal};
    if (const auto selected = context.tools.selection().set_browser_selection(
            *context.scene.scene, context.viewport, values);
        !selected) {
        set_viewport_error(context.frame, selected.error());
    }
    state.scroll_to = state.reveal;
    state.scroll_row = state.reveal_row;
    state.reveal.reset();
    state.requested_mesh_tab = resources;
    projection.bulk_expansion.reset();
    projection.search.fill('\0');
    projection.filter = BrowserFilter::all;
    projection.dirty = true;
}

void advance_reveal(BrowserPanelContext& context)
{
    auto& state = context.frame.hierarchy.browser;
    if (!state.reveal || state.reveal_scene != context.scene.scene->id()) {
        return;
    }
    const bool resources = std::holds_alternative<MeshId>(*state.reveal) ||
                           std::holds_alternative<PrimitiveId>(*state.reveal);
    if (!(resources ? state.meshes_built : state.objects_built)) {
        return;
    }
    auto& projection = resources ? state.meshes : state.objects;
    if (!state.reveal_row && !locate_reveal(state, projection)) {
        return;
    }
    if (open_reveal_parents(state, projection)) {
        commit_reveal(context, projection, resources);
    }
}

void include_anchor(ImGuiListClipper& clipper, ImGuiMultiSelectIO& io,
                    const BrowserProjection& projection)
{
    const auto anchor = visible_position(projection, io.RangeSrcItem);
    if (anchor) {
        clipper.IncludeItemByIndex(static_cast<int>(*anchor));
    } else if (io.RangeSrcItem >= 0) {
        io.RangeSrcReset = true;
    }
}

void include_revealed(BrowserPanelContext& context, const BrowserProjection& projection,
                      bool resources, ImGuiListClipper& clipper)
{
    auto& state = context.frame.hierarchy.browser;
    if (!state.scroll_to) {
        return;
    }
    const bool target_resources = std::holds_alternative<MeshId>(*state.scroll_to) ||
                                  std::holds_alternative<PrimitiveId>(*state.scroll_to);
    if (target_resources != resources) {
        return;
    }
    if (!state.scroll_row || *state.scroll_row >= projection.rows.size()) {
        return;
    }
    if (projection.rows[*state.scroll_row].key != state.scroll_to) {
        request_browser_reveal(state, *state.scroll_to, context.scene.scene->id());
        state.scroll_to.reset();
        state.scroll_row.reset();
        return;
    }
    const auto position =
        visible_position(projection, static_cast<ImGuiSelectionUserData>(*state.scroll_row));
    if (position) {
        clipper.IncludeItemByIndex(static_cast<int>(*position));
    }
}
} // namespace

void apply_browser_selection_requests(BrowserPanelContext& context,
                                      const BrowserProjection& projection,
                                      const ImGuiMultiSelectIO& io, bool resources)
{
    apply_requests(context, projection, io, resources);
}

void draw_browser_rows(BrowserPanelContext& context, BrowserProjection& projection, bool resources)
{
    if (!ImGui::BeginChild("##rows", {0, 0}, ImGuiChildFlags_None,
                           ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }
    const auto count = static_cast<int>(projection.visible.size()); // append_row enforces int limit
    constexpr auto flags =
        ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid;
    auto* io = ImGui::BeginMultiSelect(flags, -1, count);
    apply_browser_selection_requests(context, projection, *io, resources);
    ImGuiListClipper clipper;
    const float height = ImGui::GetFrameHeight();
    clipper.Begin(count, height + ImGui::GetStyle().ItemSpacing.y);
    include_anchor(clipper, *io, projection);
    include_revealed(context, projection, resources, clipper);
    while (clipper.Step()) {
        for (int position = clipper.DisplayStart; position < clipper.DisplayEnd; ++position) {
            draw_browser_row(context, projection,
                             projection.visible[static_cast<std::size_t>(position)], height);
        }
    }
    apply_browser_selection_requests(context, projection, *ImGui::EndMultiSelect(), resources);
    if (count == 0) {
        ImGui::TextDisabled("No matching items.");
    }
    ImGui::EndChild();
}

void draw_browser_tabs(BrowserPanelContext& context)
{
    auto& state = context.frame.hierarchy.browser;
    if (ImGui::BeginTabBar("##projections")) {
        const auto requested = state.requested_mesh_tab;
        if (ImGui::BeginTabItem("Objects", nullptr,
                                requested == false ? ImGuiTabItemFlags_SetSelected : 0)) {
            show_projection(context, state.objects, false, state.objects_built);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Meshes", nullptr,
                                requested == true ? ImGuiTabItemFlags_SetSelected : 0)) {
            if (state.retained_document) {
                show_projection(context, state.meshes, true, state.meshes_built);
            } else {
                ImGui::TextWrapped(
                    "Document resources are unavailable for this runtime-only Scene.");
            }
            ImGui::EndTabItem();
        }
        state.requested_mesh_tab.reset();
        ImGui::EndTabBar();
    }
}

namespace {
[[nodiscard]] bool projection_pending(const BrowserProjection& projection)
{
    return projection.dirty || projection.bulk_expansion.has_value() ||
           projection.phase != BrowserProjection::Phase::complete;
}

[[nodiscard]] bool browser_service_ready(const DocumentBrowserState& state)
{
    return state.build == DocumentBrowserState::Build::complete &&
           !projection_pending(state.objects) &&
           (!state.retained_document || !projection_pending(state.meshes)) &&
           state.visibility.phase == BrowserVisibilityCache::Phase::complete &&
           (!state.retained_document ||
            state.resource_visibility.phase == BrowserVisibilityCache::Phase::complete);
}

[[nodiscard]] Result<bool> service_browser_frame(DocumentBrowserState& state, Scene& scene,
                                                 const Viewport& viewport)
{
    const auto started = std::chrono::steady_clock::now();
    // A count cap also bounds a frame if clock resolution is coarse. Time
    // controls presentation scheduling only, never the resulting hierarchy.
    for (std::size_t chunk = 0; chunk < 64; ++chunk) {
        auto advanced = advance_document_browser(state, scene, 128);
        if (!advanced) {
            return advanced.error();
        }
        if (state.objects_built) {
            advance_browser_projection(state.objects, state, scene, 128);
            advance_browser_visibility(state, scene, 128);
        }
        if (state.meshes_built) {
            advance_browser_projection(state.meshes, state, scene, 128);
            advance_browser_resource_visibility(state, scene, viewport, 128);
        }
        if (browser_service_ready(state)) {
            return true;
        }
        if (std::chrono::steady_clock::now() - started >= std::chrono::milliseconds{2}) {
            break;
        }
    }
    return false;
}
} // namespace

void build_document_browser(ViewerFrameContext& frame, SceneSession& scene, Viewport& viewport,
                            ToolCoordinator& tools, ViewerCommandDispatcher& commands)
{
    auto& state = frame.hierarchy.browser;
    const auto advanced = service_browser_frame(state, *scene.scene, viewport);
    if (!advanced) {
        state.error = advanced.error();
        ImGui::TextWrapped("Document hierarchy unavailable: %s", advanced.error().message());
        return;
    }
    if (state.retained_document && !state.index.ready()) {
        ImGui::TextDisabled("Indexing Document...");
        return;
    }
    // One Engine/ImGui lifetime; its monotonically issued Scene value scopes
    // Document ordinals even after session cleanup resets the local cache.
    const auto scope =
        std::to_string(state.scene.debug_value()) + "/" + std::to_string(state.generation);
    ImGui::PushID(scope.c_str());
    BrowserPanelContext context{frame, scene, viewport, tools, commands};
    advance_reveal(context);
    draw_browser_tabs(context);
    ImGui::PopID();
}
} // namespace elf3d::viewer
