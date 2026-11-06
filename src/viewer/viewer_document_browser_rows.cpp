#include "viewer_document_browser.hpp"

#include <limits>
#include <utility>

namespace elf3d::viewer {
namespace {
[[nodiscard]] Result<std::size_t> append_row(BrowserProjection& projection, BrowserRow row)
{
    if (projection.rows.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return Error{ErrorCode::resource_limit_exceeded, "Scene Browser row limit exceeded"};
    }
    const auto index = projection.rows.size();
    projection.rows.push_back(std::move(row));
    return index;
}

[[nodiscard]] Result<void> walk_step(DocumentBrowserState& state)
{
    auto& frame = state.walk.back();
    const auto node = state.index.node(frame.node).value();
    if (!frame.row) {
        BrowserRow row;
        row.kind = BrowserRowKind::node;
        row.key = frame.node;
        row.parent = frame.parent;
        row.occurrence = frame.occurrence;
        row.depth = frame.depth;
        row.expandable = !node.children.empty();
        row.expanded = frame.depth == 1;
        const auto added = append_row(state.objects, std::move(row));
        if (!added) {
            return added.error();
        }
        frame.row = added.value();
    } else if (frame.child < node.children.size()) {
        if (frame.depth == std::numeric_limits<std::uint32_t>::max()) {
            return Error{ErrorCode::resource_limit_exceeded, "Scene Browser depth limit exceeded"};
        }
        const auto child = node.children[frame.child++];
        state.walk.push_back({child, *frame.row, frame.occurrence, frame.depth + 1});
    } else {
        state.walk.pop_back();
    }
    return {};
}

[[nodiscard]] Result<void> scenes_step(DocumentBrowserState& state)
{
    if (state.cursor == state.index.scene_count()) {
        state.build = DocumentBrowserState::Build::outside;
        state.cursor = 0;
        state.link = 0;
        state.group.reset();
        return {};
    }
    const auto scene = state.index.scene_at(state.cursor).value();
    if (!state.group) {
        BrowserRow row;
        row.kind = BrowserRowKind::scene;
        row.source_index = state.cursor;
        row.occurrence = scene.id.debug_value();
        row.expandable = !scene.roots.empty();
        row.expanded = state.index.default_scene() == scene.id;
        if (!state.index.default_scene()) {
            row.expanded = state.cursor == 0;
        }
        const auto added = append_row(state.objects, std::move(row));
        if (!added) {
            return added.error();
        }
        state.group = added.value();
    } else if (state.link < scene.roots.size()) {
        state.walk.push_back({scene.roots[state.link++], *state.group, scene.id.debug_value(), 1});
    } else {
        ++state.cursor;
        state.link = 0;
        state.group.reset();
    }
    return {};
}

[[nodiscard]] Result<void> outside_step(DocumentBrowserState& state)
{
    if (state.cursor == state.index.node_count()) {
        state.build = DocumentBrowserState::Build::runtime;
        state.cursor = 0;
        state.group.reset();
        return {};
    }
    const auto node = state.index.node_at(state.cursor++).value();
    if (node.in_authored_scene || node.parent) {
        return {};
    }
    if (!state.group) {
        BrowserRow row;
        row.kind = BrowserRowKind::outside;
        row.expandable = true;
        const auto added = append_row(state.objects, std::move(row));
        if (!added) {
            return added.error();
        }
        state.group = added.value();
    }
    state.walk.push_back({node.id, *state.group, 0, 1});
    return {};
}

void record_active_user(DocumentBrowserState& state, Scene& scene, NodeId node, EntityId entity)
{
    const auto source = state.index.node(node).value();
    if (!source.mesh) {
        return;
    }
    const auto primitive = scene.document_primitive(entity, 0);
    if (!primitive || !primitive.value()) {
        return;
    }
    const auto geometry = state.index.primitive(*primitive.value());
    if (geometry && geometry.value().mesh == *source.mesh) {
        ++state.active_mesh_users[*source.mesh];
    }
}

[[nodiscard]] Result<void> runtime_step(DocumentBrowserState& state, Scene& scene)
{
    if (state.cursor == state.runtime_index.size()) {
        state.objects_built = true;
        state.objects.dirty = true;
        state.build = DocumentBrowserState::Build::meshes;
        state.cursor = state.meshes_built ? state.index.mesh_count() : 0;
        state.group.reset();
        return {};
    }
    const auto item = state.runtime_index.item_at(state.cursor++).value();
    const auto node = scene.document_node(item.entity);
    if (!node) {
        return node.error();
    }
    if (node.value()) {
        record_active_user(state, scene, *node.value(), item.entity);
        return {};
    }
    if (!state.group) {
        BrowserRow group;
        group.kind = BrowserRowKind::runtime_group;
        group.expandable = true;
        group.expanded = !state.retained_document;
        const auto added = append_row(state.objects, std::move(group));
        if (!added) {
            return added.error();
        }
        state.group = added.value();
        state.runtime_begin = added.value();
    }
    BrowserRow row;
    row.kind = BrowserRowKind::runtime;
    row.key = item.entity;
    row.parent = state.group;
    row.depth = 1;
    if (item.parent) {
        const auto found = state.runtime_rows.find(*item.parent);
        if (found != state.runtime_rows.end()) {
            row.parent = found->second;
            auto& parent = state.objects.rows[found->second];
            parent.expandable = true;
            row.depth = parent.depth + 1;
        }
    }
    const auto added = append_row(state.objects, std::move(row));
    if (!added) {
        return added.error();
    }
    state.runtime_rows.emplace(item.entity, added.value());
    return {};
}

[[nodiscard]] Result<void> meshes_step(DocumentBrowserState& state)
{
    if (state.cursor == state.index.mesh_count()) {
        if (!state.runtime_rows.empty()) {
            state.runtime_rows.erase(state.runtime_rows.begin());
            return {};
        }
        state.meshes_built = true;
        state.build = DocumentBrowserState::Build::complete;
        state.meshes.dirty = true;
        return {};
    }
    const auto mesh = state.index.mesh_at(state.cursor).value();
    if (!state.group) {
        BrowserRow row;
        row.kind = BrowserRowKind::mesh;
        row.key = mesh.id;
        row.source_index = state.cursor;
        row.expandable = !mesh.primitives.empty();
        const auto added = append_row(state.meshes, std::move(row));
        if (!added) {
            return added.error();
        }
        state.group = added.value();
    } else if (state.link < mesh.primitives.size()) {
        BrowserRow row;
        row.kind = BrowserRowKind::primitive;
        row.key = mesh.primitives[state.link];
        row.source_index = state.link++;
        row.parent = state.group;
        row.depth = 1;
        const auto added = append_row(state.meshes, std::move(row));
        if (!added) {
            return added.error();
        }
    } else {
        ++state.cursor;
        state.link = 0;
        state.group.reset();
    }
    return {};
}

[[nodiscard]] Result<void> build_step(DocumentBrowserState& state, Scene& scene)
{
    if (!state.walk.empty()) {
        return walk_step(state);
    }
    switch (state.build) {
    case DocumentBrowserState::Build::scenes:
        return scenes_step(state);
    case DocumentBrowserState::Build::outside:
        return outside_step(state);
    case DocumentBrowserState::Build::runtime:
        return runtime_step(state, scene);
    case DocumentBrowserState::Build::meshes:
        return meshes_step(state);
    case DocumentBrowserState::Build::complete:
        return {};
    }
    return {};
}

[[nodiscard]] Result<void> initialize_browser(DocumentBrowserState& state, Scene& scene,
                                              std::optional<DocumentView> source)
{
    if (state.generation == std::numeric_limits<std::uint64_t>::max()) {
        fatal_error("Scene Browser generation exhausted");
    }
    const auto generation = state.generation + 1;
    const auto reveal =
        state.reveal_scene == scene.id() ? state.reveal : std::optional<BrowserSelectionKey>{};
    state = {};
    state.generation = generation;
    state.scene = scene.id();
    state.runtime_revision = scene.hierarchy_revision();
    state.retained_document = source.has_value();
    state.build =
        source ? DocumentBrowserState::Build::scenes : DocumentBrowserState::Build::runtime;
    if (reveal) {
        request_browser_reveal(state, *reveal, scene.id());
    }
    if (!source) {
        return {};
    }
    auto index = DocumentHierarchySnapshot::create(*source);
    if (!index) {
        return index.error();
    }
    state.index = std::move(index).value();
    return {};
}

void restart_runtime_rows(DocumentBrowserState& state)
{
    if (state.build < DocumentBrowserState::Build::runtime) {
        return; // authored rows and their bounded walk do not depend on runtime names
    }
    if (state.runtime_begin) {
        state.objects.rows.resize(*state.runtime_begin);
    }
    state.runtime_begin.reset();
    state.objects.visible.clear();
    state.objects.pending_visible.clear();
    state.objects.ready = false;
    state.objects.dirty = true;
    state.objects_built = false;
    state.visibility.started = false;
    state.runtime_rows.clear();
    state.active_mesh_users.clear();
    state.build = DocumentBrowserState::Build::runtime;
    state.cursor = 0;
    state.link = 0;
    state.group.reset();
    if (!state.meshes_built) {
        state.meshes.rows.clear();
    }
}

[[nodiscard]] Result<void> refresh_runtime_index(DocumentBrowserState& state, Scene& scene)
{
    auto runtime = SceneHierarchySnapshot::create(scene);
    if (!runtime) {
        return runtime.error();
    }
    if (state.runtime_ready) {
        restart_runtime_rows(state);
    }
    state.runtime_index = std::move(runtime).value();
    state.runtime_revision = scene.hierarchy_revision();
    state.runtime_ready = false;
    return {};
}
[[nodiscard]] Result<void> ensure_source(DocumentBrowserState& state, Scene& scene,
                                         std::optional<DocumentView> source)
{
    if (state.scene != scene.id() || state.retained_document != source.has_value() ||
        (source && !state.index.matches_document(*source))) {
        if (const auto started = initialize_browser(state, scene, source); !started) {
            return started.error();
        }
    }
    if (state.error) {
        return *state.error;
    }
    return {};
}

[[nodiscard]] Result<bool> prepare_index(DocumentBrowserState& state, DocumentView source,
                                         std::size_t units)
{
    if (!state.index.ready()) {
        const auto advanced = state.index.advance(source, units);
        if (!advanced) {
            state.error = advanced.error();
            return advanced.error();
        }
        return false;
    }
    return true;
}

[[nodiscard]] Result<bool> prepare_runtime(DocumentBrowserState& state, Scene& scene,
                                           std::size_t units)
{
    if (state.runtime_ready && state.runtime_revision == scene.hierarchy_revision()) {
        return true;
    }
    if (!state.runtime_index.matches_scene(scene)) {
        const auto runtime = refresh_runtime_index(state, scene);
        if (!runtime) {
            return runtime.error();
        }
        return false;
    }
    const auto advanced = state.runtime_index.advance(scene, units);
    if (!advanced) {
        return advanced.error();
    }
    state.runtime_ready = advanced.value();
    return false;
}

[[nodiscard]] Result<bool> drive_rows(DocumentBrowserState& state, Scene& scene, std::size_t units)
{
    for (std::size_t unit = 0; unit < units && state.build != DocumentBrowserState::Build::complete;
         ++unit) {
        if (const auto advanced = build_step(state, scene); !advanced) {
            state.error = advanced.error();
            return advanced.error();
        }
    }
    return state.build == DocumentBrowserState::Build::complete;
}
} // namespace

Result<bool> advance_document_browser(DocumentBrowserState& state, Scene& scene, std::size_t units)
{
    const auto source = scene.document();
    if (units == 0) {
        return Error{ErrorCode::invalid_argument, "Scene Browser requires a nonzero work budget"};
    }
    if (const auto checked = ensure_source(state, scene, source); !checked) {
        return checked.error();
    }
    if (source) {
        auto index = prepare_index(state, *source, units);
        if (!index || !index.value()) {
            return index;
        }
    }
    auto runtime = prepare_runtime(state, scene, units);
    if (!runtime || !runtime.value()) {
        return runtime;
    }
    return drive_rows(state, scene, units);
}

Result<void> adopt_document_browser_index(DocumentBrowserState& state, const Scene& scene,
                                          DocumentHierarchySnapshot&& index)
{
    const auto document = scene.document();
    if (!document || !index.ready() || !index.matches_document(*document)) {
        return Error{ErrorCode::invalid_argument,
                     "Scene Browser index does not match its Document"};
    }
    if (state.generation == std::numeric_limits<std::uint64_t>::max()) {
        fatal_error("Scene Browser generation exhausted");
    }
    const auto generation = state.generation + 1;
    state = {};
    state.scene = scene.id();
    state.generation = generation;
    state.runtime_revision = scene.hierarchy_revision();
    state.retained_document = true;
    state.index = std::move(index);
    return {};
}
} // namespace elf3d::viewer
