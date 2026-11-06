#include "viewer_tools.hpp"

#include <algorithm>
#include <new>
#include <utility>

namespace elf3d::viewer {
namespace {
[[nodiscard]] Result<BrowserSelectionKey> canonical_key(Scene& scene, NodeId node)
{
    const auto document = scene.document();
    if (!document || !document->node(node)) {
        return Error{ErrorCode::invalid_node_id, "Selection node is outside the current Document"};
    }
    return BrowserSelectionKey{node};
}
[[nodiscard]] Result<BrowserSelectionKey> canonical_key(Scene& scene, EntityId entity)
{
    const auto node = scene.document_node(entity);
    if (!node) {
        return node.error();
    }
    return node.value() ? BrowserSelectionKey{*node.value()} : BrowserSelectionKey{entity};
}
[[nodiscard]] Result<BrowserSelectionKey> canonical_key(Scene& scene, MeshId mesh)
{
    const auto document = scene.document();
    if (!document || !document->mesh(mesh)) {
        return Error{ErrorCode::invalid_mesh_id, "Selection mesh is outside the current Document"};
    }
    return BrowserSelectionKey{mesh};
}
[[nodiscard]] Result<BrowserSelectionKey> canonical_key(Scene& scene, PrimitiveId primitive)
{
    const auto document = scene.document();
    if (!document || !document->primitive(primitive)) {
        return Error{ErrorCode::invalid_primitive_id,
                     "Selection primitive is outside the current Document"};
    }
    return BrowserSelectionKey{primitive};
}

[[nodiscard]] bool resource_key(const BrowserSelectionKey& key) noexcept
{
    return std::holds_alternative<MeshId>(key) || std::holds_alternative<PrimitiveId>(key);
}
[[nodiscard]] bool key_less(const BrowserSelectionKey& left, const BrowserSelectionKey& right)
{
    if (left.index() != right.index()) {
        return left.index() < right.index();
    }
    const auto ordinal = [](auto id) { return id.debug_value(); };
    return std::visit(ordinal, left) < std::visit(ordinal, right);
}
[[nodiscard]] Result<std::optional<EntityId>> active_entity(Scene& scene,
                                                            const BrowserSelectionKey& key)
{
    if (const auto* node = std::get_if<NodeId>(&key)) {
        return scene.entity_for_document_node(*node);
    }
    if (const auto* entity = std::get_if<EntityId>(&key)) {
        return std::optional<EntityId>{*entity};
    }
    return std::optional<EntityId>{};
}
[[nodiscard]] Result<void> normalize_keys(Scene& scene, std::vector<BrowserSelectionKey>& values)
{
    std::optional<bool> resources;
    for (auto& value : values) {
        auto checked = std::visit([&](auto id) { return canonical_key(scene, id); }, value);
        if (!checked) {
            return checked.error();
        }
        value = checked.value();
        const bool resource = resource_key(value);
        if (resources && *resources != resource) {
            return Error{ErrorCode::invalid_argument,
                         "Object and resource selections have separate scopes"};
        }
        resources = resource;
    }
    return {};
}
[[nodiscard]] Result<std::vector<EntityId>>
active_entities(Scene& scene, std::span<const BrowserSelectionKey> values)
{
    std::vector<EntityId> result;
    result.reserve(values.size());
    for (const auto& key : values) {
        const auto entity = active_entity(scene, key);
        if (!entity) {
            return entity.error();
        }
        if (entity.value()) {
            result.push_back(*entity.value());
        }
    }
    std::sort(result.begin(), result.end(),
              [](EntityId a, EntityId b) { return a.debug_value() < b.debug_value(); });
    return result;
}

struct PreparedBrowserSelection {
    std::vector<BrowserSelectionKey> keys;
    std::optional<BrowserSelectionKey> primary;
    std::vector<EntityId> active;
    std::optional<EntityId> viewport_primary;
};

[[nodiscard]] Result<PreparedBrowserSelection>
prepare_selection(Scene& scene, std::vector<BrowserSelectionKey> replacement)
{
    if (const auto checked = normalize_keys(scene, replacement); !checked) {
        return checked.error();
    }
    PreparedBrowserSelection result;
    if (!replacement.empty()) {
        result.primary = replacement.back();
        const auto primary = active_entity(scene, *result.primary);
        if (!primary) {
            return primary.error();
        }
        result.viewport_primary = primary.value();
    }
    std::sort(replacement.begin(), replacement.end(), key_less);
    replacement.erase(std::unique(replacement.begin(), replacement.end()), replacement.end());
    auto active = active_entities(scene, replacement);
    if (!active) {
        return active.error();
    }
    result.active = std::move(active).value();
    if (!result.viewport_primary && !result.active.empty()) {
        result.viewport_primary = result.active.front();
    }
    result.keys = std::move(replacement);
    return result;
}

[[nodiscard]] std::size_t project_entries(std::span<SelectionTool::Entry> entries,
                                          std::span<const EntityId> active)
{
    std::size_t count = 0;
    for (auto& entry : entries) {
        entry.selected = std::binary_search(
            active.begin(), active.end(), entry.entity,
            [](EntityId a, EntityId b) { return a.debug_value() < b.debug_value(); });
        count += entry.selected ? 1U : 0U;
    }
    return count;
}
} // namespace

void SelectionTool::refresh_browser_projection(Scene& scene)
{
    resource_feedback_dirty_ = true;
    std::vector<BrowserSelectionKey> retained;
    retained.reserve(browser_selection_.size());
    for (const auto& value : browser_selection_) {
        auto checked = std::visit([&](auto id) { return canonical_key(scene, id); }, value);
        if (checked) {
            retained.push_back(checked.value());
        }
    }
    auto prepared = prepare_selection(scene, std::move(retained));
    if (!prepared) {
        browser_selection_.clear();
        browser_primary_.reset();
        selected_count_ = project_entries(entries_, {});
        return;
    }
    auto& state = prepared.value();
    selected_count_ = project_entries(entries_, state.active);
    browser_selection_ = std::move(state.keys);
    if (!browser_primary_ || !browser_selected(*browser_primary_)) {
        browser_primary_ = state.primary;
    }
}

BrowserSelectionDomain SelectionTool::browser_domain() const noexcept
{
    return browser_domain_;
}
std::span<const BrowserSelectionKey> SelectionTool::browser_selection() const noexcept
{
    return browser_selection_;
}
std::optional<BrowserSelectionKey> SelectionTool::browser_primary() const noexcept
{
    return browser_primary_;
}
bool SelectionTool::browser_selected(const BrowserSelectionKey& key) const noexcept
{
    const auto found =
        std::lower_bound(browser_selection_.begin(), browser_selection_.end(), key, key_less);
    return found != browser_selection_.end() && *found == key;
}

Result<void>
SelectionTool::set_browser_selection(Scene& scene, Viewport& viewport,
                                     std::span<const BrowserSelectionKey> values) noexcept
{
    try {
        // Copy before synchronize: the caller may be borrowing our current keys.
        std::vector<BrowserSelectionKey> replacement(values.begin(), values.end());
        if (const auto synced = synchronize(scene); !synced) {
            return synced.error();
        }
        auto prepared = prepare_selection(scene, std::move(replacement));
        if (!prepared) {
            return prepared.error();
        }
        auto& state = prepared.value();
        if (state.viewport_primary) {
            if (const auto selected = viewport.set_selected_entity(scene, *state.viewport_primary);
                !selected) {
                return selected.error();
            }
        } else {
            viewport.clear_selection();
        }
        selected_count_ = project_entries(entries_, state.active);
        if (!state.keys.empty()) {
            browser_domain_ = resource_key(state.keys.front()) ? BrowserSelectionDomain::resources
                                                               : BrowserSelectionDomain::objects;
        }
        browser_selection_ = std::move(state.keys);
        browser_primary_ = state.primary;
        resource_feedback_dirty_ = true;
        return {};
    } catch (const std::bad_alloc&) {
        fatal_error("Browser selection allocation failed");
    } catch (...) {
        fatal_error("Unexpected browser selection exception");
    }
}

Result<void> SelectionTool::apply_membership(Scene& scene, Viewport& viewport,
                                             BrowserSelectionKey value, bool additive) noexcept
{
    try {
        const auto key = std::visit([&](auto id) { return canonical_key(scene, id); }, value);
        if (!key) {
            return key.error();
        }
        std::vector<BrowserSelectionKey> replacement;
        const bool same_domain =
            (browser_domain_ == BrowserSelectionDomain::resources) == resource_key(key.value());
        if (additive && same_domain) {
            replacement = browser_selection_;
        }
        const auto found = std::find(replacement.begin(), replacement.end(), key.value());
        if (additive && found != replacement.end()) {
            replacement.erase(found);
        } else {
            replacement.push_back(key.value());
        }
        return set_browser_selection(scene, viewport, replacement);
    } catch (const std::bad_alloc&) {
        fatal_error("Object selection allocation failed");
    } catch (...) {
        fatal_error("Unexpected object selection exception");
    }
}

Result<void> SelectionTool::select_all(Scene& scene, Viewport& viewport) noexcept
{
    try {
        if (const auto synced = synchronize(scene); !synced) {
            return synced.error();
        }
        std::vector<BrowserSelectionKey> values;
        values.reserve(entries_.size());
        for (const auto& entry : entries_) {
            if (!entry.camera_only) {
                values.emplace_back(entry.entity);
            }
        }
        return set_browser_selection(scene, viewport, values);
    } catch (const std::bad_alloc&) {
        fatal_error("Select All allocation failed");
    } catch (...) {
        fatal_error("Unexpected Select All exception");
    }
}
} // namespace elf3d::viewer
