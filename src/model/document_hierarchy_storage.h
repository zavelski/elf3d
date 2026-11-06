#pragma once

#include <elf3d/model.h>

#include <string>
#include <vector>

namespace elf3d {

class DocumentHierarchySnapshot::Impl final {
  public:
    struct SceneRecord {
        DocumentSceneId id;
        std::string name;
        std::vector<NodeId> roots;
    };
    struct NodeRecord {
        NodeId id;
        std::string name;
        std::optional<NodeId> parent;
        std::vector<NodeId> children;
        std::optional<MeshId> mesh;
        bool camera = false;
        bool light = false;
        bool reachable = false;
    };
    struct MeshRecord {
        MeshId id;
        std::string name;
        std::vector<PrimitiveId> primitives;
        std::vector<NodeId> users;
        std::size_t user_count = 0;
    };
    enum class Phase {
        scenes,
        nodes,
        meshes,
        primitives,
        count_users,
        reserve_users,
        fill_users,
        seed_reachable,
        visit_reachable,
        complete
    };

    model::detail::DocumentOwnerToken owner;
    std::uint64_t revision = 0;
    std::optional<DocumentSceneId> default_scene;
    std::vector<SceneRecord> scenes;
    std::vector<NodeRecord> nodes;
    std::vector<MeshRecord> meshes;
    std::vector<DocumentHierarchyPrimitive> primitives;
    std::vector<NodeId> reachable_queue;
    Phase phase = Phase::scenes;
    std::size_t cursor = 0;
    std::size_t link_cursor = 0;

    [[nodiscard]] Result<void> step(DocumentView source);
    [[nodiscard]] Result<void> structural_step(DocumentView source);
    [[nodiscard]] Result<void> scene_step(DocumentView source);
    [[nodiscard]] Result<void> node_step(DocumentView source);
    [[nodiscard]] Result<void> mesh_step(DocumentView source);
    void primitive_step(DocumentView source);
    void usage_step();
    void reachable_step();
    void next(Phase value) noexcept;
    void mark_reachable(NodeId node);
};

} // namespace elf3d
