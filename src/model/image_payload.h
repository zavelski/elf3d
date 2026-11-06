#ifndef ELF3D_MODEL_DETAIL_IMAGE_PAYLOAD_H
#define ELF3D_MODEL_DETAIL_IMAGE_PAYLOAD_H

#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace elf3d::model::detail {

// Private CPU resource lifetime boundary: independently destroyed Documents may
// retain the same immutable bytes. There are no owner/back references or GPU
// resources or mutable aliases.
class ImagePayload final {
  public:
    ImagePayload() noexcept = default;

    [[nodiscard]] static ImagePayload copy(std::span<const std::byte> pixels,
                                           std::span<const std::byte> source)
    {
        ImagePayload result;
        result.bytes_ = std::make_shared<const Bytes>(
            Bytes{{pixels.begin(), pixels.end()}, {source.begin(), source.end()}});
        return result;
    }

    [[nodiscard]] static ImagePayload adopt(std::vector<std::byte>&& pixels,
                                            std::vector<std::byte>&& source)
    {
        ImagePayload result;
        result.bytes_ = std::make_shared<const Bytes>(Bytes{std::move(pixels), std::move(source)});
        return result;
    }

    [[nodiscard]] ImagePayload independent_copy() const
    {
        return copy(pixels(), source_bytes());
    }

    [[nodiscard]] std::span<const std::byte> pixels() const noexcept
    {
        return bytes_ ? std::span<const std::byte>{bytes_->pixels} : std::span<const std::byte>{};
    }

    [[nodiscard]] std::span<const std::byte> source_bytes() const noexcept
    {
        return bytes_ ? std::span<const std::byte>{bytes_->source} : std::span<const std::byte>{};
    }

  private:
    struct Bytes {
        std::vector<std::byte> pixels;
        std::vector<std::byte> source;
    };

    std::shared_ptr<const Bytes> bytes_;
};

enum class ImageCopyPolicy { independent, share_immutable };

} // namespace elf3d::model::detail

#endif
