#pragma once

#include "exporter_internal.hpp"
#include <model/document_handle_access.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace elf3d::gltf::exporter_detail {
template <typename View, typename Id>
[[nodiscard]] std::optional<std::size_t> export_table_index(const std::vector<View>& table,
                                                            Id id) noexcept
{
    const auto value = model::detail::DocumentHandleAccess::value(id);
    if (value == 0 || value > table.size())
        return std::nullopt;
    const auto index = static_cast<std::size_t>(value - 1);
    return table[index].id == id ? std::optional{index} : std::nullopt;
}
[[nodiscard]] Result<void> validate_export_assets(const ExportData& document);
} // namespace elf3d::gltf::exporter_detail
