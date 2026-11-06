#pragma once

#include "resource_retirement_fake.h"
#include <graphics/cube_preparation.h>
#include <utility>

namespace elf3d::graphics::tests {
class FakeCubePreparation final : public TextureCubePreparation {
  public:
    FakeCubePreparation(TextureCubeStorageDescription storage,
                        std::unique_ptr<TextureCube> texture) noexcept
        : storage_(storage), texture_(std::move(texture))
    {
    }
    [[nodiscard]] Result<CubeUploadRequest>
    request(std::size_t maximum_bytes) const noexcept override
    {
        if (!texture_ || retiring_ || maximum_bytes == 0)
            return Error{ErrorCode::invalid_argument, "Fake cubemap is unavailable"};
        CubeUploadRequest next;
        next.mip = mip_;
        next.face = face_;
        next.region.phase = phase_;
        if (phase_ == TexturePreparationPhase::pixels) {
            auto region =
                texture_upload_region(cube_face_storage(storage_, mip_), x_, y_, maximum_bytes);
            if (!region)
                return region.error();
            next.region = region.value();
        }
        next.region.driver_call_limit = phase_ == TexturePreparationPhase::ready ? 0 : 1;
        return next;
    }
    [[nodiscard]] Result<TextureUploadStep>
    advance(std::span<const std::byte> pixels) noexcept override
    {
        const auto next =
            request(phase_ == TexturePreparationPhase::pixels ? pixels.size()
                                                              : maximum_texture_upload_chunk);
        if (!next)
            return next.error();
        if (pixels.size() != next.value().region.bytes)
            return Error{ErrorCode::invalid_argument, "Fake cubemap payload is incomplete"};
        if (phase_ == TexturePreparationPhase::ready)
            return TextureUploadStep{0, 0, true};
        std::size_t allocated = 0;
        if (phase_ == TexturePreparationPhase::storage) {
            allocated = texture_storage_bytes(cube_face_storage(storage_, mip_)).value();
            phase_ = TexturePreparationPhase::pixels;
        } else if (phase_ == TexturePreparationPhase::pixels) {
            advance_pixels(next.value().region);
        } else {
            phase_ = TexturePreparationPhase::ready;
        }
        ++calls_;
        return TextureUploadStep{pixels.size(), 1, phase_ == TexturePreparationPhase::ready,
                                 allocated};
    }
    [[nodiscard]] std::uint64_t driver_calls() const noexcept override
    {
        return calls_;
    }
    [[nodiscard]] Result<ResourceRetirementStep>
    advance_retirement(const ResourceRetirementBudget& budget) noexcept override
    {
        if (!texture_)
            return ResourceRetirementStep{0, 0, true, false};
        auto step = retire_fake_objects(retirement_objects_, budget);
        if (step.value().progressed || step.value().complete)
            retiring_ = true;
        return step;
    }
    [[nodiscard]] Result<std::unique_ptr<TextureCube>> take_texture() noexcept override
    {
        if (!texture_ || retiring_ || phase_ != TexturePreparationPhase::ready)
            return Error{ErrorCode::invalid_argument, "Fake cubemap is incomplete or consumed"};
        return std::move(texture_);
    }

  private:
    void advance_pixels(const TextureUploadRequest& region) noexcept
    {
        const auto extent = cube_face_storage(storage_, mip_).extent;
        x_ += region.extent.width;
        if (x_ == extent.width) {
            x_ = 0;
            y_ += region.extent.height;
        }
        if (y_ != extent.height)
            return;
        y_ = 0;
        if (++face_ == 6) {
            face_ = 0;
            ++mip_;
        }
        phase_ = mip_ == storage_.mip_count ? TexturePreparationPhase::validation
                                            : TexturePreparationPhase::storage;
    }
    TextureCubeStorageDescription storage_;
    std::unique_ptr<TextureCube> texture_;
    TexturePreparationPhase phase_ = TexturePreparationPhase::storage;
    bool retiring_ = false;
    std::uint32_t retirement_objects_ = 1;
    std::uint32_t mip_ = 0;
    std::uint32_t face_ = 0;
    std::uint32_t x_ = 0;
    std::uint32_t y_ = 0;
    std::uint64_t calls_ = 0;
};
} // namespace elf3d::graphics::tests
