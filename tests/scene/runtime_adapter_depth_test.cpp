#include <elf3d/model.h>
#include <elf3d/scene.h>

#include <assets/assets.h>
#include <cstddef>
#include <optional>
#include <scene/scene.h>
#include <utility>

namespace {

[[nodiscard]] bool has_document_root(elf3d::Document& document,
                                     const elf3d::Result<elf3d::DocumentSceneId>& scene,
                                     const elf3d::Result<elf3d::NodeId>& root)
{
    return scene && root && document.add_scene_root(scene.value(), root.value());
}

[[nodiscard]] bool has_expected_depth(const elf3d::SceneHierarchyStatistics& statistics,
                                      std::size_t depth) noexcept
{
    return statistics.entities == depth && statistics.root_entities == 1U &&
           statistics.maximum_depth == depth - 1U;
}

[[nodiscard]] bool valid_mapping(elf3d::scene::Storage& storage, elf3d::NodeId node)
{
    const auto mapped = storage.entity_for_document_node(node);
    if (!mapped || !mapped.value() || storage.document_node(*mapped.value()).value() != node) {
        return false;
    }
    elf3d::Document foreign;
    const auto foreign_node = foreign.create_node().value();
    const auto runtime_only = storage.create_entity().value();
    if (storage.entity_for_document_node(foreign_node) ||
        storage.document_node(runtime_only).value() || !storage.destroy_entity(*mapped.value())) {
        return false;
    }
    return !storage.entity_for_document_node(node).value() &&
           !storage.document_node(*mapped.value());
}

} // namespace

int elf3d_scene_runtime_adapter_depth_test()
{
    constexpr std::size_t hierarchy_depth = 5120U;
    elf3d::Document document;
    const auto document_scene = document.create_scene("Deep hierarchy");
    const auto root = document.create_node();
    if (!has_document_root(document, document_scene, root)) {
        return 1;
    }

    elf3d::NodeId parent = root.value();

    for (std::size_t index = 1U; index < hierarchy_depth; ++index) {
        const auto child = document.create_node();
        if (!child || !document.set_parent(child.value(), parent)) {
            return 2;
        }
        parent = child.value();
    }
    if (elf3d::validate_document(document.view()).has_errors()) {
        return 3;
    }

    const elf3d::SceneId scene_id = elf3d::detail::SceneHandleAccess::create_scene(29U, 1U);
    elf3d::scene::Storage runtime_scene{scene_id};
    if (!elf3d::scene::populate_from_document(std::move(document), document_scene.value(),
                                              runtime_scene)) {
        return 4;
    }

    const elf3d::SceneHierarchyStatistics statistics = runtime_scene.hierarchy_statistics();
    return has_expected_depth(statistics, hierarchy_depth) && valid_mapping(runtime_scene, parent)
               ? 0
               : 5;
}
