#ifndef ELF3D_TOOLS_CLIPPING_CLIPPING_CONTROLLER_H
#define ELF3D_TOOLS_CLIPPING_CLIPPING_CONTROLLER_H

#include <elf3d/clipping/filter.h>
#include <elf3d/measurement.h>
#include <elf3d/scene/storage.h>

#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace elf3d::tools::clipping {

struct ClippingOverlay {
    std::array<OverlayLineSegment, 4 + maximum_clipping_boxes * 12> lines;
    std::size_t line_count = 0;

    [[nodiscard]] std::span<const OverlayLineSegment> line_span() const noexcept {
        return std::span<const OverlayLineSegment>{lines.data(), line_count};
    }
};

class ClippingController final {
  public:
    [[nodiscard]] Result<void> set_section_plane(const SectionPlane &plane) noexcept;
    void clear_section_plane() noexcept;

    [[nodiscard]] Result<std::uint32_t> add_box(const ClippingBox &box);
    [[nodiscard]] Result<void> set_box(std::uint32_t index, const ClippingBox &box) noexcept;
    [[nodiscard]] Result<void> remove_box(std::uint32_t index) noexcept;
    void clear_boxes() noexcept;
    void clear() noexcept;

    [[nodiscard]] Result<void> set_helpers_visible(bool visible) noexcept;
    [[nodiscard]] Result<void>
    set_helper_settings(const ClippingHelperSettings &settings) noexcept;

    [[nodiscard]] Result<void> reset_box_to_visible_bounds(const scene::Storage &scene,
                                                           const scene::VisibilityFilter &visibility,
                                                           std::uint32_t index) noexcept;
    [[nodiscard]] Result<std::uint32_t>
    add_box_from_visible_bounds(const scene::Storage &scene,
                                const scene::VisibilityFilter &visibility);

    [[nodiscard]] ClippingSnapshot snapshot() const noexcept;
    [[nodiscard]] Result<elf3d::clipping::ClippingFilter> filter() const;
    [[nodiscard]] std::uint64_t revision() const noexcept;

    [[nodiscard]] Result<ClippingOverlay> overlay(Bounds3 helper_bounds) const noexcept;

  private:
    void increment_revision() noexcept;
    [[nodiscard]] Result<ClippingBox>
    box_from_visible_bounds(const scene::Storage &scene,
                            const scene::VisibilityFilter &visibility) const noexcept;

    SectionPlane section_plane_;
    std::array<ClippingBox, maximum_clipping_boxes> boxes_;
    std::uint32_t box_count_ = 0;
    ClippingHelperSettings helper_settings_;
    std::uint64_t revision_ = 0;
};

[[nodiscard]] Bounds3 visible_bounds(const scene::Storage &scene,
                                     const scene::VisibilityFilter &visibility,
                                     const elf3d::clipping::ClippingFilter &filter) noexcept;

} // namespace elf3d::tools::clipping

#endif
