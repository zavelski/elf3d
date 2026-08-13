module;

#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/scene.h>

#include <array>
#include <cstdint>
#include <optional>

export module elf.clipping.runtime;

import elf.clipping;
import elf.core;
import elf.scene;

export namespace elf3d::clipping_runtime {

class ClippingController final {
  public:
    [[nodiscard]] Result<void> set_section_plane(const SectionPlane& plane) noexcept;
    void clear_section_plane() noexcept;

    [[nodiscard]] Result<std::uint32_t> add_box(const ClippingBox& box);
    [[nodiscard]] Result<void> set_box(std::uint32_t index, const ClippingBox& box) noexcept;
    [[nodiscard]] Result<void> remove_box(std::uint32_t index) noexcept;
    void clear_boxes() noexcept;
    void clear() noexcept;

    [[nodiscard]] ClippingSnapshot snapshot() const noexcept;
    [[nodiscard]] Result<elf3d::clipping::ClippingFilter> filter() const;
    [[nodiscard]] std::uint64_t revision() const noexcept;

  private:
    void increment_revision() noexcept;

    SectionPlane section_plane_;
    std::array<ClippingBox, maximum_clipping_boxes> boxes_;
    std::uint32_t box_count_ = 0;
    std::uint64_t revision_ = 0;
};

[[nodiscard]] std::optional<Bounds3>
visible_bounds(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
               const elf3d::clipping::ClippingFilter& filter) noexcept;

} // namespace elf3d::clipping_runtime
