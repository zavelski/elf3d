#pragma once
#include "resource_retirement_fake.h"

#include <graphics/texture_preparation.h>
#include <utility>

namespace elf3d::graphics::tests {
class FakeTexturePreparation final : public Texture2DPreparation {
  public:
    FakeTexturePreparation(TextureStorageDescription storage,
                           std::unique_ptr<Texture2D> texture) noexcept
        : storage_(storage), texture_(std::move(texture))
    {
    }
    [[nodiscard]] Result<TextureUploadRequest>
    request(std::size_t maximum_bytes) const noexcept override
    {
        if (!texture_ || retiring_)
            return Error{ErrorCode::invalid_argument, "Fake texture preparation was consumed"};
        if (maximum_bytes == 0)
            return Error{ErrorCode::invalid_argument, "Fake texture allowance must be positive"};
        if (phase_ == TexturePreparationPhase::pixels) {
            auto region = texture_upload_region(storage_, x_, y_, maximum_bytes);
            if (region)
                region.value().driver_call_limit = 1;
            return region;
        }
        TextureUploadRequest next;
        next.phase = phase_;
        next.driver_call_limit = phase_ == TexturePreparationPhase::ready ? 0 : 1;
        return next;
    }
    [[nodiscard]] Result<TextureUploadStep>
    advance(std::span<const std::byte> payload) noexcept override
    {
        const auto next =
            request(phase_ == TexturePreparationPhase::pixels ? payload.size()
                                                              : maximum_texture_upload_chunk);
        if (!next)
            return next.error();
        if (payload.size() != next.value().bytes || payload.size() > maximum_texture_upload_chunk)
            return Error{ErrorCode::invalid_argument, "Unexpected fake texture payload"};
        if (phase_ == TexturePreparationPhase::ready)
            return TextureUploadStep{0, 0, true};
        std::size_t allocated = 0;
        if (phase_ == TexturePreparationPhase::storage) {
            const auto bytes = texture_storage_bytes(storage_);
            if (!bytes)
                return bytes.error();
            allocated = bytes.value();
            phase_ = storage_.initialization == TextureInitialization::pixels
                         ? TexturePreparationPhase::pixels
                         : TexturePreparationPhase::validation;
        } else if (phase_ == TexturePreparationPhase::pixels) {
            advance_pixels(next.value());
        } else {
            phase_ = TexturePreparationPhase::ready;
        }
        ++calls_;
        return TextureUploadStep{payload.size(), 1, phase_ == TexturePreparationPhase::ready,
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
    [[nodiscard]] Result<std::unique_ptr<Texture2D>> take_texture() noexcept override
    {
        if (!texture_ || phase_ != TexturePreparationPhase::ready || retiring_)
            return Error{ErrorCode::invalid_argument, "Fake texture is incomplete or consumed"};
        return std::move(texture_);
    }

  private:
    void advance_pixels(const TextureUploadRequest& next) noexcept
    {
        x_ += next.extent.width;
        if (x_ == storage_.extent.width) {
            x_ = 0;
            y_ += next.extent.height;
        }
        if (y_ == storage_.extent.height)
            phase_ = TexturePreparationPhase::validation;
    }
    TextureStorageDescription storage_;
    std::uint32_t retirement_objects_ = 1;
    bool retiring_ = false;
    std::unique_ptr<Texture2D> texture_;
    TexturePreparationPhase phase_ = TexturePreparationPhase::storage;
    std::uint32_t x_ = 0;
    std::uint32_t y_ = 0;
    std::uint64_t calls_ = 0;
};
} // namespace elf3d::graphics::tests
