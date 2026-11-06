#pragma once

#include "exporter_internal.hpp"

#include <elf3d/core/assert.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace elf3d::gltf::exporter_detail {
// Keeps sub-four-byte input fragments across packets, so BinaryBuilder's
// four-byte alignment never inserts padding inside one logical index stream.
class ExportIndexOutput final {
  public:
    ExportIndexOutput() noexcept = default;
    explicit ExportIndexOutput(std::uint32_t maximum) noexcept
        : maximum_(maximum), type_(index_component_type(maximum)),
          stride_(type_ == 5121U ? 1U : (type_ == 5123U ? 2U : 4U))
    {
    }
    [[nodiscard]] std::uint32_t component_type() const noexcept
    {
        return type_;
    }
    [[nodiscard]] Result<void> append(std::span<const std::uint32_t> values, BinaryBuilder& binary)
    {
        if (finished_)
            return Error{ErrorCode::invalid_argument, "Index output is already complete"};
        for (const auto value : values) {
            if (value > maximum_ || value == std::numeric_limits<std::uint32_t>::max())
                return Error{ErrorCode::mesh_index_out_of_range,
                             "Index exceeds the captured output maximum"};
            for (std::size_t byte = 0; byte < stride_; ++byte)
                bytes_[used_++] = static_cast<std::byte>((value >> (byte * 8)) & 0xffU);
            if (used_ == bytes_.size()) {
                if (const auto written = flush(binary); !written)
                    return written.error();
            }
        }
        return {};
    }
    [[nodiscard]] Result<ByteRange> finish(BinaryBuilder& binary)
    {
        if (const auto written = flush(binary); !written)
            return written.error();
        if (first_)
            return Error{ErrorCode::invalid_mesh_data, "Index output has no elements"};
        finished_ = true;
        return range_;
    }

  private:
    [[nodiscard]] Result<void> flush(BinaryBuilder& binary)
    {
        if (used_ == 0)
            return {};
        const auto part = binary.append_bytes(std::span{bytes_}.first(used_));
        if (!part)
            return part.error();
        if (first_) {
            range_ = part.value();
            first_ = false;
        } else {
            ELF3D_ASSERT(part.value().offset == range_.offset + range_.length);
            range_.length += part.value().length;
        }
        used_ = 0;
        return {};
    }
    std::array<std::byte, 4096> bytes_;
    std::size_t used_ = 0;
    std::uint32_t maximum_ = 0;
    std::uint32_t type_ = 5125U;
    std::size_t stride_ = 4;
    ByteRange range_;
    bool first_ = true;
    bool finished_ = false;
};
} // namespace elf3d::gltf::exporter_detail
