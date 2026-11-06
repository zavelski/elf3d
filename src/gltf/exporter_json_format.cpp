#include "exporter_json_internal.hpp"

#include "json_document.h"

#include <string>
#include <string_view>
#include <utility>

namespace elf3d::gltf::exporter_detail {

Result<FormattedJson> format_json(std::string_view source, ModelNumericWritePolicy numeric_policy)
{
    // A complete glTF includes all geometry/accessor records, independently of
    // retained metadata. Keep metadata callers on their original parser budget.
    constexpr std::size_t file_multiplier = sizeof(void*) == 8 ? 4 : 1;
    auto parsed = json::parse(source, {json::maximum_json_bytes * file_multiplier,
                                       json::maximum_json_values * file_multiplier});
    if (!parsed) {
        return Error{parsed.error().code(),
                     "Full glTF JSON (" + std::to_string(source.size()) +
                         " bytes): " + std::string{parsed.error().message()}};
    }
    const std::size_t cleaned = numeric_policy == ModelNumericWritePolicy::clean_rotation
                                    ? json::clean_rotation_matrices(parsed.value())
                                    : 0U;
    json::omit_defaults(parsed.value());
    if (const auto names = json::prepare_file_output(parsed.value()); !names) {
        return names.error();
    }
    json::reconcile_extensions(parsed.value());
    json::order_sections(parsed.value());
    auto written = json::write(parsed.value());
    if (!written) {
        return written.error();
    }
    return FormattedJson{std::move(written).value(), cleaned};
}

} // namespace elf3d::gltf::exporter_detail
