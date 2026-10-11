#include "exporter_json_internal.hpp"

#include "json_document.h"

#include <string>
#include <string_view>
#include <utility>

namespace elf3d::gltf::exporter_detail {

Result<FormattedJson> format_json(std::string_view source, ModelNumericWritePolicy numeric_policy)
{
    auto parsed = json::parse(source);
    if (!parsed) {
        return parsed.error();
    }
    const std::size_t cleaned = numeric_policy == ModelNumericWritePolicy::clean_rotation
                                    ? json::clean_rotation_matrices(parsed.value())
                                    : 0U;
    json::omit_defaults(parsed.value());
    json::reconcile_extensions(parsed.value());
    json::order_sections(parsed.value());
    auto written = json::write(parsed.value());
    if (!written) {
        return written.error();
    }
    return FormattedJson{std::move(written).value(), cleaned};
}

} // namespace elf3d::gltf::exporter_detail
