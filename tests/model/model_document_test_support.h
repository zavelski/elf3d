#pragma once

#include <array>
#include <cstddef>

namespace elf3d::model::tests {

[[nodiscard]] inline std::array<std::byte, 4> single_pixel()
{
    return {std::byte{17}, std::byte{34}, std::byte{51}, std::byte{255}};
}

} // namespace elf3d::model::tests

[[nodiscard]] int test_model_asset_views_and_statistics();
[[nodiscard]] int test_model_asset_rejections();
