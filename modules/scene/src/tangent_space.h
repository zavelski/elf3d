#pragma once

#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include <cstdint>
#include <optional>

namespace elf3d::scene::tangent_detail {

// Builds owned runtime geometry without modifying the canonical vertex streams.
// An empty result retains geometric-normal shading when no usable basis exists.
[[nodiscard]] Result<std::optional<PrimitiveData>>
prepare_tangent_geometry(const PrimitiveDataView& source, std::uint32_t texcoord_set);

} // namespace elf3d::scene::tangent_detail
