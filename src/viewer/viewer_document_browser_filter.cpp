#include "viewer_document_browser.hpp"

#include <algorithm>
#include <utility>

namespace elf3d::viewer {
namespace {
[[nodiscard]] char folded(char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}
[[nodiscard]] bool contains(std::string_view value, std::string_view query)
{
    return query.empty() ||
           std::search(value.begin(), value.end(), query.begin(), query.end(),
                       [](char a, char b) { return folded(a) == folded(b); }) != value.end();
}
[[nodiscard]] bool filtered(const BrowserProjection& projection) noexcept
{
    return projection.search.front() != '\0' || projection.filter != BrowserFilter::all;
}

[[nodiscard]] bool mesh_filter(DocumentHierarchyMesh mesh, BrowserFilter filter)
{
    if (filter == BrowserFilter::shared) {
        return mesh.users.size() > 1;
    }
    return filter == BrowserFilter::unused && mesh.users.empty();
}
[[nodiscard]] bool node_filter(DocumentHierarchyNode node, const DocumentBrowserState& state,
                               BrowserFilter filter)
{
    if (filter == BrowserFilter::cameras) {
        return node.has_camera;
    }
    if (filter == BrowserFilter::lights) {
        return node.has_areal_light;
    }
    return node.mesh && mesh_filter(state.index.mesh(*node.mesh).value(), filter);
}
[[nodiscard]] bool type_filter(const BrowserRow& row, const DocumentBrowserState& state,
                               const Scene& scene, BrowserFilter filter)
{
    if (filter == BrowserFilter::all) {
        return true;
    }
    if (!row.key) {
        return false;
    }
    if (const auto* node = std::get_if<NodeId>(&*row.key)) {
        return node_filter(state.index.node(*node).value(), state, filter);
    }
    if (const auto* mesh = std::get_if<MeshId>(&*row.key)) {
        return mesh_filter(state.index.mesh(*mesh).value(), filter);
    }
    if (const auto* primitive = std::get_if<PrimitiveId>(&*row.key)) {
        const auto source = state.index.primitive(*primitive).value();
        return mesh_filter(state.index.mesh(source.mesh).value(), filter);
    }
    const auto info = scene.entity_info(browser_row_id<EntityId>(row));
    return info && filter == BrowserFilter::cameras && info.value().has_camera;
}

[[nodiscard]] bool node_query(const DocumentBrowserState& state, NodeId id, std::string_view query)
{
    const auto node = state.index.node(id).value();
    if (node.has_camera && contains("camera", query)) {
        return true;
    }
    if (node.has_areal_light && contains("light", query)) {
        return true;
    }
    if (node.mesh) {
        return contains("mesh", query) ||
               contains(state.index.mesh(*node.mesh).value().name, query);
    }
    return false;
}
[[nodiscard]] bool row_query(const BrowserRow& row, const DocumentBrowserState& state,
                             const Scene& scene, std::string_view query)
{
    if (contains(browser_row_name(state, row, scene), query) ||
        contains(browser_row_type(row), query)) {
        return true;
    }
    if (!row.key) {
        return false;
    }
    if (const auto* node = std::get_if<NodeId>(&*row.key)) {
        return node_query(state, *node, query);
    }
    if (const auto* entity = std::get_if<EntityId>(&*row.key)) {
        const auto info = scene.entity_info(*entity);
        return info && info.value().has_camera && contains("camera", query);
    }
    return false;
}

void match_step(BrowserProjection& projection, const DocumentBrowserState& state,
                const Scene& scene)
{
    if (projection.cursor == projection.rows.size()) {
        projection.phase = BrowserProjection::Phase::ancestors;
        return;
    }
    auto& row = projection.rows[projection.cursor++];
    row.included = type_filter(row, state, scene, projection.filter) &&
                   row_query(row, state, scene, projection.search.data());
}

void ancestor_step(BrowserProjection& projection)
{
    if (projection.cursor == 0) {
        projection.phase = BrowserProjection::Phase::visible;
        return;
    }
    const auto& row = projection.rows[--projection.cursor];
    if (row.included && row.parent) {
        projection.rows[*row.parent].included = true;
    }
}

void visible_step(BrowserProjection& projection)
{
    if (projection.cursor == projection.rows.size()) {
        projection.visible.swap(projection.pending_visible);
        projection.pending_visible.clear();
        projection.ready = true;
        projection.phase = BrowserProjection::Phase::complete;
        return;
    }
    auto& row = projection.rows[projection.cursor];
    row.shown = row.included;
    if (row.parent) {
        const auto& parent = projection.rows[*row.parent];
        row.shown = row.shown && parent.shown && (parent.expanded || filtered(projection));
    }
    if (row.shown) {
        projection.pending_visible.push_back(projection.cursor);
    }
    ++projection.cursor;
}

void projection_step(BrowserProjection& projection, const DocumentBrowserState& state,
                     const Scene& scene)
{
    switch (projection.phase) {
    case BrowserProjection::Phase::match:
        match_step(projection, state, scene);
        break;
    case BrowserProjection::Phase::ancestors:
        ancestor_step(projection);
        break;
    case BrowserProjection::Phase::visible:
        visible_step(projection);
        break;
    case BrowserProjection::Phase::complete:
        break;
    }
}
} // namespace

std::string_view browser_row_name(const DocumentBrowserState& state, const BrowserRow& row,
                                  const Scene& scene)
{
    switch (row.kind) {
    case BrowserRowKind::scene:
        return state.index.scene_at(row.source_index).value().name;
    case BrowserRowKind::node:
        return state.index.node(browser_row_id<NodeId>(row)).value().name;
    case BrowserRowKind::mesh:
        return state.index.mesh(browser_row_id<MeshId>(row)).value().name;
    case BrowserRowKind::outside:
        return "Outside Scenes";
    case BrowserRowKind::runtime_group:
        return "Runtime Objects";
    case BrowserRowKind::runtime: {
        const auto name = scene.entity_name(browser_row_id<EntityId>(row));
        return name ? name.value() : std::string_view{};
    }
    case BrowserRowKind::primitive:
        return {};
    }
    return {};
}

const char* browser_row_type(const BrowserRow& row) noexcept
{
    switch (row.kind) {
    case BrowserRowKind::scene:
        return "Scene";
    case BrowserRowKind::node:
        return "Node";
    case BrowserRowKind::outside:
        return "Outside Scenes";
    case BrowserRowKind::runtime_group:
        return "Runtime Objects";
    case BrowserRowKind::runtime:
        return "Runtime Object";
    case BrowserRowKind::mesh:
        return "Mesh";
    case BrowserRowKind::primitive:
        return "Primitive";
    }
    return "Object";
}

void advance_browser_projection(BrowserProjection& projection, const DocumentBrowserState& state,
                                const Scene& scene, std::size_t units)
{
    if (projection.bulk_expansion) {
        for (std::size_t unit = 0; unit < units && projection.bulk_cursor < projection.rows.size();
             ++unit) {
            projection.rows[projection.bulk_cursor++].expanded = *projection.bulk_expansion;
        }
        if (projection.bulk_cursor == projection.rows.size()) {
            projection.bulk_expansion.reset();
            projection.dirty = true;
        }
        return;
    }
    if (projection.dirty) {
        projection.cursor = 0;
        projection.phase = BrowserProjection::Phase::match;
        projection.pending_visible.clear();
        projection.dirty = false;
    }
    for (std::size_t unit = 0;
         unit < units && projection.phase != BrowserProjection::Phase::complete; ++unit) {
        projection_step(projection, state, scene);
    }
}

void request_browser_reveal(DocumentBrowserState& state, BrowserSelectionKey key, SceneId source)
{
    state.reveal = std::move(key);
    state.reveal_scene = source;
    state.reveal_cursor = 0;
    state.reveal_row.reset();
    state.reveal_parent.reset();
}
} // namespace elf3d::viewer
