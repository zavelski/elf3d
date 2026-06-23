#ifndef ELF3D_CLIPPING_FILTER_H
#define ELF3D_CLIPPING_FILTER_H

#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/math/conventions.h>

#include <array>
#include <span>

namespace elf3d::clipping {

inline constexpr float clipping_boundary_epsilon = 0.00001F;
inline constexpr float minimum_clipping_box_extent = 0.00001F;

enum class BoundsClassification {
    outside,
    intersecting,
    inside,
};

struct ClippingFilter {
    bool section_plane_enabled = false;
    Float3 section_plane_normal{0.0F, 1.0F, 0.0F};
    float section_plane_offset = 0.0F;
    bool retain_positive_half_space = true;
    std::array<Bounds3, maximum_clipping_boxes> boxes{};
    std::uint32_t enabled_box_count = 0;
    std::uint64_t revision = 0;

    [[nodiscard]] bool has_clipping() const noexcept {
        return section_plane_enabled || enabled_box_count != 0;
    }
};

[[nodiscard]] Result<SectionPlane> normalized_section_plane(const SectionPlane &plane) noexcept;
[[nodiscard]] Result<ClippingBox> validated_clipping_box(const ClippingBox &box) noexcept;
[[nodiscard]] Result<ClippingFilter>
make_filter(const SectionPlane &section_plane, std::span<const ClippingBox> boxes,
            std::uint64_t revision);
[[nodiscard]] ClippingFilter disabled_filter() noexcept;

[[nodiscard]] bool is_valid_bounds(Bounds3 bounds) noexcept;
[[nodiscard]] Bounds3 transform_bounds(Bounds3 local_bounds, const math::Matrix4 &world) noexcept;
[[nodiscard]] bool contains_point(const ClippingFilter &filter, Float3 world_position) noexcept;
[[nodiscard]] BoundsClassification classify_bounds(const ClippingFilter &filter,
                                                   Bounds3 world_bounds) noexcept;
[[nodiscard]] Bounds3 clipped_bounds(const ClippingFilter &filter,
                                     Bounds3 world_bounds) noexcept;

} // namespace elf3d::clipping

#endif
