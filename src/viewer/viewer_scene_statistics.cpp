#include "viewer_scene_session.hpp"

#include <cstddef>
#include <cstdint>

namespace elf3d::viewer {
std::uint64_t document_instance_triangles(DocumentView document) noexcept
{
    std::uint64_t triangles = 0;
    for (std::size_t index = 0; index < document.node_count(); ++index) {
        const auto node = document.node_at(index).value();
        if (!node.mesh) {
            continue;
        }
        for (PrimitiveId primitive : document.mesh(*node.mesh).value().primitives) {
            triangles += document.primitive(primitive).value().data.indices.size() / 3;
        }
    }
    return triangles;
}
} // namespace elf3d::viewer
