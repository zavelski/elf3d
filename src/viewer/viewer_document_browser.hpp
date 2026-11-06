#pragma once

#include "viewer_tools.hpp"
#include <elf3d/core/assert.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace elf3d::viewer {
struct ViewerFrameContext;
struct SceneSession;
class ViewerCommandDispatcher;

enum class BrowserRowKind : std::uint8_t {
    scene,
    node,
    outside,
    runtime_group,
    runtime,
    mesh,
    primitive
};
enum class BrowserFilter : std::uint8_t { all, shared, unused, cameras, lights };
enum class BrowserVisibility : std::uint8_t { unavailable, visible, hidden, mixed };

struct BrowserVisibilityCache {
    enum class Phase { capture, ancestors, complete };
    std::vector<BrowserVisibility> values;
    std::vector<BrowserVisibility> pending;
    std::uint64_t revision = 0;
    std::size_t cursor = 0;
    Phase phase = Phase::capture;
    bool started = false;
};

struct BrowserResourceVisibility {
    BrowserVisibility local = BrowserVisibility::unavailable;
    BrowserVisibility effective = BrowserVisibility::unavailable;
};
struct BrowserResourceVisibilityCache {
    std::vector<BrowserResourceVisibility> values;
    std::vector<BrowserResourceVisibility> pending;
    std::uint64_t scene_revision = 0;
    std::uint64_t view_revision = 0;
    std::uint64_t hierarchy_revision = 0;
    std::size_t row = 0;
    std::size_t user = 0;
    BrowserResourceVisibility accumulated;
    BrowserVisibilityCache::Phase phase = BrowserVisibilityCache::Phase::capture;
    bool started = false;
};

// Presentation coordinates and typed selection keys only; names stay in the
// single owned engine index. Parent is a row coordinate, never a domain ID.
struct BrowserRow {
    BrowserRowKind kind = BrowserRowKind::node;
    std::optional<BrowserSelectionKey> key;
    std::optional<std::size_t> parent;
    std::size_t source_index = 0;
    std::uint64_t occurrence = 0;
    std::uint32_t depth = 0;
    bool expandable = false;
    bool expanded = false;
    bool included = false;
    bool shown = false;
};

template <class Id> [[nodiscard]] Id browser_row_id(const BrowserRow& row) noexcept
{
    const auto* value = row.key ? std::get_if<Id>(&*row.key) : nullptr;
    ELF3D_ASSERT(value != nullptr);
    return *value;
}

struct BrowserProjection {
    enum class Phase { match, ancestors, visible, complete };
    std::vector<BrowserRow> rows;
    std::vector<std::size_t> visible;
    std::vector<std::size_t> pending_visible;
    std::array<char, 256> search{};
    BrowserFilter filter = BrowserFilter::all;
    Phase phase = Phase::match;
    std::size_t cursor = 0;
    bool ready = false;
    bool dirty = true;
    std::optional<bool> bulk_expansion;
    std::size_t bulk_cursor = 0;
};

struct BrowserWalkFrame {
    NodeId node;
    std::size_t parent = 0;
    std::uint64_t occurrence = 0;
    std::uint32_t depth = 0;
    std::size_t child = 0;
    std::optional<std::size_t> row;
};

struct DocumentBrowserState {
    enum class Build { scenes, outside, runtime, meshes, complete };
    SceneId scene;
    std::uint64_t generation = 0;
    std::uint64_t runtime_revision = 0;
    bool runtime_ready = false;
    bool retained_document = false;
    bool objects_built = false;
    bool meshes_built = false;
    DocumentHierarchySnapshot index;
    SceneHierarchySnapshot runtime_index;
    std::optional<Error> error;
    BrowserProjection objects;
    BrowserProjection meshes;
    BrowserVisibilityCache visibility;
    BrowserResourceVisibilityCache resource_visibility;
    std::vector<BrowserWalkFrame> walk;
    struct EntityLess {
        bool operator()(EntityId a, EntityId b) const noexcept
        {
            return a.debug_value() < b.debug_value(); // one state.scene owner
        }
    };
    std::map<EntityId, std::size_t, EntityLess> runtime_rows;
    struct MeshLess {
        bool operator()(MeshId a, MeshId b) const noexcept
        {
            return a.debug_value() < b.debug_value();
        }
    };
    std::map<MeshId, std::uint64_t, MeshLess> active_mesh_users;
    Build build = Build::scenes;
    std::size_t cursor = 0;
    std::size_t link = 0;
    std::optional<std::size_t> group;
    std::optional<std::size_t> runtime_begin;
    std::optional<BrowserSelectionKey> reveal;
    SceneId reveal_scene;
    std::optional<BrowserSelectionKey> scroll_to;
    std::optional<std::size_t> scroll_row;
    std::size_t reveal_cursor = 0;
    std::optional<std::size_t> reveal_row;
    std::optional<std::size_t> reveal_parent;
    std::optional<bool> requested_mesh_tab;
};

[[nodiscard]] Result<bool> advance_document_browser(DocumentBrowserState& state, Scene& scene,
                                                    std::size_t units = 256);
[[nodiscard]] Result<void> adopt_document_browser_index(DocumentBrowserState& state,
                                                        const Scene& scene,
                                                        DocumentHierarchySnapshot&& index);
void advance_browser_projection(BrowserProjection& projection, const DocumentBrowserState& state,
                                const Scene& scene, std::size_t units = 512);
void advance_browser_visibility(DocumentBrowserState& state, const Scene& scene,
                                std::size_t units = 512);
[[nodiscard]] BrowserVisibility browser_subtree_visibility(const DocumentBrowserState& state,
                                                           const Scene& scene,
                                                           std::size_t row_index) noexcept;
[[nodiscard]] std::string_view browser_row_name(const DocumentBrowserState& state,
                                                const BrowserRow& row, const Scene& scene);
[[nodiscard]] const char* browser_row_type(const BrowserRow& row) noexcept;
[[nodiscard]] Result<std::vector<PrimitiveInstance>>
browser_resource_instances(const Scene& scene, const DocumentHierarchySnapshot& index,
                           std::span<const BrowserSelectionKey> keys);
struct BrowserResourceRequest {
    EntityId camera;
    BrowserSelectionKey key;
    BrowserResourceAction action;
};
[[nodiscard]] Result<void> apply_browser_resource_action(Scene& scene, Viewport& viewport,
                                                         const DocumentHierarchySnapshot& index,
                                                         const BrowserResourceRequest& request);
void advance_browser_resource_visibility(DocumentBrowserState& state, const Scene& scene,
                                         const Viewport& viewport, std::size_t units = 512);
[[nodiscard]] BrowserResourceVisibility
browser_resource_visibility(const DocumentBrowserState& state, const Scene& scene,
                            const Viewport& viewport, std::size_t row) noexcept;
void request_browser_reveal(DocumentBrowserState& state, BrowserSelectionKey key, SceneId source);
void build_document_browser(ViewerFrameContext& frame, SceneSession& scene, Viewport& viewport,
                            ToolCoordinator& tools, ViewerCommandDispatcher& commands);
[[nodiscard]] bool draw_browser_properties(ViewerFrameContext& frame, const SceneSession& scene,
                                           SelectionTool& selection);
} // namespace elf3d::viewer
