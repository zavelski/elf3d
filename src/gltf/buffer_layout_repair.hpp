#pragma once

#include <cstddef>
#include <span>

namespace elf3d::gltf::importer_detail {

struct GlbBufferViewLayout {
    std::size_t offset = 0U;
    std::size_t size = 0U;
};

struct GlbBufferLayoutRepair {
    std::size_t buffer_size = 0U;
    std::size_t repaired_fields = 0U;
};

[[nodiscard]] GlbBufferLayoutRepair
recover_signed_glb_buffer_layout(std::size_t bin_size,
                                 std::span<GlbBufferViewLayout> views) noexcept;

} // namespace elf3d::gltf::importer_detail
