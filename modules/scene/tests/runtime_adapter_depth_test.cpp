#include <elf3d/model.h>
#include <elf3d/scene.h>

#include <cstddef>
#include <optional>
#include <utility>

import elf.assets;
import elf.model;
import elf.scene;

int elf3d_scene_runtime_adapter_depth_test() {
    constexpr std::size_t hierarchy_depth = 1024U;
    elf3d::Document document;
    const auto document_scene = document.create_scene("Deep hierarchy");
    const auto root = document.create_node();
    if (!document_scene || !root ||
        !document.add_scene_root(document_scene.value(), root.value())) {
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
    return statistics.entities == hierarchy_depth && statistics.root_entities == 1U &&
                   statistics.maximum_depth == hierarchy_depth - 1U
               ? 0
               : 5;
}
