#pragma once

#include <elf3d/core/result.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace elf3d::renderer::tests {

constexpr std::size_t studio_environment_header_bytes = 64;
constexpr std::size_t studio_environment_payload_bytes = 1'622'000;

inline void write_studio_u32(std::vector<std::byte>& bytes, std::size_t offset,
                             std::uint32_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] =
            static_cast<std::byte>((value >> static_cast<std::uint32_t>(index * 8U)) & 0xFFU);
    }
}

inline void write_studio_u64(std::vector<std::byte>& bytes, std::size_t offset,
                             std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] =
            static_cast<std::byte>((value >> static_cast<std::uint32_t>(index * 8U)) & 0xFFU);
    }
}

[[nodiscard]] inline std::uint64_t studio_fnv1a64(std::span<const std::byte> bytes) noexcept {
    std::uint64_t value = 14'695'981'039'346'656'037ULL;
    for (const std::byte byte : bytes) {
        value ^= std::to_integer<std::uint8_t>(byte);
        value *= 1'099'511'628'211ULL;
    }
    return value;
}

[[nodiscard]] inline std::vector<std::byte> valid_studio_environment_bytes() {
    std::vector<std::byte> result(studio_environment_header_bytes +
                                  studio_environment_payload_bytes);
    constexpr std::array<std::byte, 8> magic{
        std::byte{'E'}, std::byte{'L'}, std::byte{'F'}, std::byte{'3'},
        std::byte{'D'}, std::byte{'I'}, std::byte{'B'}, std::byte{'L'},
    };
    std::copy(magic.begin(), magic.end(), result.begin());
    write_studio_u32(result, 8, 3);
    write_studio_u32(result, 12, 64);
    write_studio_u32(result, 16, 1);
    write_studio_u32(result, 20, 6);
    write_studio_u32(result, 24, 32);
    write_studio_u32(result, 28, 1);
    write_studio_u32(result, 32, 128);
    write_studio_u32(result, 36, 8);
    write_studio_u32(result, 40, 256);
    write_studio_u32(result, 44, 256);
    write_studio_u64(result, 48, studio_environment_payload_bytes);
    write_studio_u64(result, 56,
                     studio_fnv1a64(std::span<const std::byte>{result}.subspan(
                         studio_environment_header_bytes)));
    return result;
}

class TestStudioEnvironmentSource final : public StudioEnvironmentSource {
  public:
    explicit TestStudioEnvironmentSource(std::vector<std::byte> bytes) : bytes_(std::move(bytes)) {}

    [[nodiscard]] Result<std::span<const std::byte>> bytes() noexcept override {
        ++read_count_;
        return std::span<const std::byte>{bytes_};
    }

    [[nodiscard]] int read_count() const noexcept {
        return read_count_;
    }

  private:
    std::vector<std::byte> bytes_;
    int read_count_ = 0;
};

[[nodiscard]] inline std::unique_ptr<StudioEnvironmentSource>
make_test_studio_environment_source() {
    return std::make_unique<TestStudioEnvironmentSource>(valid_studio_environment_bytes());
}

} // namespace elf3d::renderer::tests
