#include <elf3d/core/error.h>
#include <elf3d/model.h>

#include "exporter_internal.hpp"
#include "exporter_json_internal.hpp"
#include "json_document.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elf3d::gltf::exporter_detail {
namespace {

[[nodiscard]] Result<void> append_camera(std::string& output, const NodeView& node,
                                         const ExportData&)
{
    const auto& camera = *node.perspective_camera;
    const auto& properties = node.camera_properties;
    output.append("{\"type\":\"perspective\",\"perspective\":{\"yfov\":");
    append_float(output, camera.vertical_field_of_view_radians);
    output.append(",\"znear\":");
    append_float(output, camera.near_plane);
    if (!properties.infinite_far_plane) {
        output.append(",\"zfar\":");
        append_float(output, camera.far_plane);
    }
    if (properties.aspect_ratio) {
        output.append(",\"aspectRatio\":");
        append_float(output, *properties.aspect_ratio);
    }
    append_preserved_metadata(output, properties.projection_metadata);
    output.push_back('}');
    if (!properties.name.empty()) {
        output.append(",\"name\":");
        append_string(output, properties.name);
    }
    append_preserved_metadata(output, properties.metadata);
    output.push_back('}');
    return {};
}

} // namespace

Result<void> append_cameras(std::string& output, const ExportData& document)
{
    bool first = true;
    for (const NodeView& node : document.nodes) {
        if (!node.perspective_camera) {
            continue;
        }
        output.append(first ? ",\"cameras\":[" : ",");
        first = false;
        if (const auto result = append_camera(output, node, document); !result) {
            return result.error();
        }
    }
    if (!first) {
        output.push_back(']');
    }
    return {};
}

} // namespace elf3d::gltf::exporter_detail
