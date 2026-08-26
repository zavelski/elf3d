module;

#include <elf3d/core/result.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

module elf.renderer;

import elf.graphics;

namespace elf3d::renderer {
namespace {

constexpr float pi = 3.14159265359F;
constexpr std::uint32_t integration_sample_count = 256;
constexpr std::uint32_t diffuse_extent = 32;
constexpr std::uint32_t specular_extent = 128;
constexpr std::uint32_t specular_mip_count = 8;
constexpr std::uint32_t brdf_extent = 256;

struct Vector3 final {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct StudioLobe final {
    Vector3 direction;
    Vector3 radiance;
    float exponent = 1.0F;
};

struct NeutralStudioDescription final {
    Vector3 zenith;
    Vector3 horizon;
    Vector3 ground;
    std::array<StudioLobe, 3> lobes;
};

// Calibrated neutral studio profile. These are the only artistic parameters in
// the environment generation path; convolution and shader structure stay fixed.
constexpr NeutralStudioDescription neutral_studio{
    {0.18F, 0.19F, 0.21F},
    {0.36F, 0.35F, 0.34F},
    {0.10F, 0.095F, 0.09F},
    {{{{-0.53F, 0.58F, 0.62F}, {3.2F, 3.05F, 2.85F}, 18.0F},
      {{0.70F, 0.22F, 0.68F}, {1.15F, 1.22F, 1.32F}, 10.0F},
      {{0.05F, 0.68F, -0.73F}, {1.75F, 1.70F, 1.62F}, 26.0F}}}};

struct CubeMipPixels final {
    std::uint32_t extent = 0;
    std::array<std::vector<std::uint16_t>, graphics::cubemap_face_count> faces;
};

[[nodiscard]] Vector3 add(Vector3 left, Vector3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] Vector3 subtract(Vector3 left, Vector3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Vector3 multiply(Vector3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] Vector3 multiply(Vector3 left, Vector3 right) noexcept {
    return {left.x * right.x, left.y * right.y, left.z * right.z};
}

[[nodiscard]] float dot(Vector3 left, Vector3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] Vector3 cross(Vector3 left, Vector3 right) noexcept {
    return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] Vector3 normalize(Vector3 value) noexcept {
    const float length_squared = dot(value, value);
    if (!std::isfinite(length_squared) || length_squared <= 0.00000001F) {
        return {0.0F, 1.0F, 0.0F};
    }
    return multiply(value, 1.0F / std::sqrt(length_squared));
}

[[nodiscard]] Vector3 mix(Vector3 left, Vector3 right, float amount) noexcept {
    return add(multiply(left, 1.0F - amount), multiply(right, amount));
}

[[nodiscard]] Vector3 environment_radiance(Vector3 direction) noexcept {
    const Vector3 normalized = normalize(direction);
    const float height = std::clamp(normalized.y, -1.0F, 1.0F);
    Vector3 result = height >= 0.0F ? mix(neutral_studio.horizon, neutral_studio.zenith, height)
                                    : mix(neutral_studio.horizon, neutral_studio.ground, -height);
    for (const StudioLobe& lobe : neutral_studio.lobes) {
        const float response =
            std::pow(std::max(dot(normalized, normalize(lobe.direction)), 0.0F), lobe.exponent);
        result = add(result, multiply(lobe.radiance, response));
    }
    return result;
}

[[nodiscard]] float radical_inverse(std::uint32_t bits) noexcept {
    bits = (bits << 16U) | (bits >> 16U);
    bits = ((bits & 0x55555555U) << 1U) | ((bits & 0xAAAAAAAAU) >> 1U);
    bits = ((bits & 0x33333333U) << 2U) | ((bits & 0xCCCCCCCCU) >> 2U);
    bits = ((bits & 0x0F0F0F0FU) << 4U) | ((bits & 0xF0F0F0F0U) >> 4U);
    bits = ((bits & 0x00FF00FFU) << 8U) | ((bits & 0xFF00FF00U) >> 8U);
    return static_cast<float>(bits) * 2.3283064365386963e-10F;
}

[[nodiscard]] std::array<float, 2> hammersley(std::uint32_t index, std::uint32_t count) noexcept {
    return {static_cast<float>(index) / static_cast<float>(count), radical_inverse(index)};
}

[[nodiscard]] Vector3 tangent_to_world(Vector3 local, Vector3 normal) noexcept {
    const Vector3 reference =
        std::abs(normal.z) < 0.999F ? Vector3{0.0F, 0.0F, 1.0F} : Vector3{1.0F, 0.0F, 0.0F};
    const Vector3 tangent = normalize(cross(reference, normal));
    const Vector3 bitangent = cross(normal, tangent);
    return normalize(add(add(multiply(tangent, local.x), multiply(bitangent, local.y)),
                         multiply(normal, local.z)));
}

[[nodiscard]] Vector3 cosine_hemisphere(std::array<float, 2> sample, Vector3 normal) noexcept {
    const float radius = std::sqrt(sample[0]);
    const float angle = 2.0F * pi * sample[1];
    const Vector3 local{radius * std::cos(angle), radius * std::sin(angle),
                        std::sqrt(std::max(1.0F - sample[0], 0.0F))};
    return tangent_to_world(local, normal);
}

[[nodiscard]] Vector3 importance_sample_ggx(std::array<float, 2> sample, Vector3 normal,
                                            float roughness) noexcept {
    const float alpha = roughness * roughness;
    const float alpha_squared = alpha * alpha;
    const float angle = 2.0F * pi * sample[0];
    const float cosine =
        std::sqrt((1.0F - sample[1]) / (1.0F + (alpha_squared - 1.0F) * sample[1]));
    const float sine = std::sqrt(std::max(1.0F - cosine * cosine, 0.0F));
    return tangent_to_world({std::cos(angle) * sine, std::sin(angle) * sine, cosine}, normal);
}

[[nodiscard]] Vector3 cube_direction(std::size_t face, std::uint32_t x, std::uint32_t y,
                                     std::uint32_t extent) noexcept {
    const float u = (2.0F * (static_cast<float>(x) + 0.5F) / static_cast<float>(extent)) - 1.0F;
    const float v = (2.0F * (static_cast<float>(y) + 0.5F) / static_cast<float>(extent)) - 1.0F;
    switch (face) {
    case 0:
        return normalize({1.0F, -v, -u});
    case 1:
        return normalize({-1.0F, -v, u});
    case 2:
        return normalize({u, 1.0F, v});
    case 3:
        return normalize({u, -1.0F, -v});
    case 4:
        return normalize({u, -v, 1.0F});
    default:
        return normalize({-u, -v, -1.0F});
    }
}

[[nodiscard]] std::uint16_t float_to_half(float value) noexcept {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    const std::uint32_t sign = (bits >> 16U) & 0x8000U;
    const std::uint32_t exponent = (bits >> 23U) & 0xFFU;
    const std::uint32_t mantissa = bits & 0x7FFFFFU;
    if (exponent == 0xFFU) {
        return static_cast<std::uint16_t>(sign | (mantissa == 0U ? 0x7C00U : 0x7E00U));
    }
    const int adjusted_exponent = static_cast<int>(exponent) - 127 + 15;
    if (adjusted_exponent <= 0) {
        if (adjusted_exponent < -10) {
            return static_cast<std::uint16_t>(sign);
        }
        const std::uint32_t significand = mantissa | 0x800000U;
        const std::uint32_t shift = static_cast<std::uint32_t>(14 - adjusted_exponent);
        return static_cast<std::uint16_t>(sign | ((significand + (1U << (shift - 1U))) >> shift));
    }
    if (adjusted_exponent >= 31) {
        return static_cast<std::uint16_t>(sign | 0x7C00U);
    }
    const std::uint32_t rounded = mantissa + 0x1000U;
    return static_cast<std::uint16_t>(
        sign | (static_cast<std::uint32_t>(adjusted_exponent) << 10U) | (rounded >> 13U));
}

void append_half_color(std::vector<std::uint16_t>& pixels, Vector3 color) {
    pixels.push_back(float_to_half(std::max(color.x, 0.0F)));
    pixels.push_back(float_to_half(std::max(color.y, 0.0F)));
    pixels.push_back(float_to_half(std::max(color.z, 0.0F)));
    pixels.push_back(float_to_half(1.0F));
}

[[nodiscard]] CubeMipPixels generate_diffuse_irradiance() {
    CubeMipPixels mip;
    mip.extent = diffuse_extent;
    for (std::size_t face = 0; face < mip.faces.size(); ++face) {
        std::vector<std::uint16_t>& pixels = mip.faces[face];
        pixels.reserve(static_cast<std::size_t>(diffuse_extent) * diffuse_extent * 4U);
        for (std::uint32_t y = 0; y < diffuse_extent; ++y) {
            for (std::uint32_t x = 0; x < diffuse_extent; ++x) {
                const Vector3 normal = cube_direction(face, x, y, diffuse_extent);
                Vector3 accumulated;
                for (std::uint32_t sample_index = 0; sample_index < integration_sample_count;
                     ++sample_index) {
                    const Vector3 direction = cosine_hemisphere(
                        hammersley(sample_index, integration_sample_count), normal);
                    accumulated = add(accumulated, environment_radiance(direction));
                }
                append_half_color(pixels, multiply(accumulated, pi / integration_sample_count));
            }
        }
    }
    return mip;
}

[[nodiscard]] CubeMipPixels generate_empty_mip(std::uint32_t extent) {
    CubeMipPixels mip;
    mip.extent = extent;
    const std::size_t value_count = static_cast<std::size_t>(extent) * extent * 4U;
    for (std::vector<std::uint16_t>& face : mip.faces) {
        face.resize(value_count);
    }
    return mip;
}

[[nodiscard]] Vector3 filtered_specular_radiance(const Vector3& reflection,
                                                 float roughness) noexcept {
    Vector3 accumulated;
    float total_weight = 0.0F;
    for (std::uint32_t sample_index = 0; sample_index < integration_sample_count; ++sample_index) {
        const Vector3 half_vector = importance_sample_ggx(
            hammersley(sample_index, integration_sample_count), reflection, roughness);
        const Vector3 light = normalize(
            subtract(multiply(half_vector, 2.0F * dot(reflection, half_vector)), reflection));
        const float weight = std::max(dot(reflection, light), 0.0F);
        if (weight > 0.0F) {
            accumulated = add(accumulated, multiply(environment_radiance(light), weight));
            total_weight += weight;
        }
    }
    return multiply(accumulated, 1.0F / std::max(total_weight, 0.001F));
}

[[nodiscard]] CubeMipPixels generate_specular_mip(std::uint32_t level) {
    CubeMipPixels mip;
    mip.extent = std::max(specular_extent >> level, 1U);
    const float roughness = static_cast<float>(level) / static_cast<float>(specular_mip_count - 1U);
    for (std::size_t face = 0; face < mip.faces.size(); ++face) {
        std::vector<std::uint16_t>& pixels = mip.faces[face];
        pixels.reserve(static_cast<std::size_t>(mip.extent) * mip.extent * 4U);
        for (std::uint32_t y = 0; y < mip.extent; ++y) {
            for (std::uint32_t x = 0; x < mip.extent; ++x) {
                const Vector3 reflection = cube_direction(face, x, y, mip.extent);
                if (level == 0) {
                    append_half_color(pixels, environment_radiance(reflection));
                    continue;
                }
                append_half_color(pixels, filtered_specular_radiance(reflection, roughness));
            }
        }
    }
    return mip;
}

[[nodiscard]] float geometry_schlick_ggx(float n_dot_v, float roughness) noexcept {
    const float k = roughness * roughness * 0.5F;
    return n_dot_v / std::max(n_dot_v * (1.0F - k) + k, 0.0001F);
}

[[nodiscard]] std::array<float, 2> integrate_brdf(float n_dot_v, float roughness) noexcept {
    const Vector3 normal{0.0F, 0.0F, 1.0F};
    const Vector3 view{std::sqrt(std::max(1.0F - n_dot_v * n_dot_v, 0.0F)), 0.0F, n_dot_v};
    float scale = 0.0F;
    float bias = 0.0F;
    for (std::uint32_t sample_index = 0; sample_index < integration_sample_count; ++sample_index) {
        const Vector3 half_vector = importance_sample_ggx(
            hammersley(sample_index, integration_sample_count), normal, roughness);
        const Vector3 light =
            normalize(subtract(multiply(half_vector, 2.0F * dot(view, half_vector)), view));
        const float n_dot_l = std::max(light.z, 0.0F);
        const float n_dot_h = std::max(half_vector.z, 0.0F);
        const float v_dot_h = std::max(dot(view, half_vector), 0.0F);
        if (n_dot_l > 0.0F) {
            const float geometry =
                geometry_schlick_ggx(n_dot_v, roughness) * geometry_schlick_ggx(n_dot_l, roughness);
            const float visibility = geometry * v_dot_h / std::max(n_dot_h * n_dot_v, 0.0001F);
            const float fresnel = std::pow(1.0F - v_dot_h, 5.0F);
            scale += (1.0F - fresnel) * visibility;
            bias += fresnel * visibility;
        }
    }
    return {scale / integration_sample_count, bias / integration_sample_count};
}

[[nodiscard]] std::vector<std::uint16_t> generate_brdf_lut() {
    std::vector<std::uint16_t> pixels;
    pixels.reserve(static_cast<std::size_t>(brdf_extent) * brdf_extent * 4U);
    for (std::uint32_t y = 0; y < brdf_extent; ++y) {
        const float roughness = (static_cast<float>(y) + 0.5F) / static_cast<float>(brdf_extent);
        for (std::uint32_t x = 0; x < brdf_extent; ++x) {
            const float n_dot_v = (static_cast<float>(x) + 0.5F) / static_cast<float>(brdf_extent);
            const std::array<float, 2> integrated = integrate_brdf(n_dot_v, roughness);
            pixels.push_back(float_to_half(integrated[0]));
            pixels.push_back(float_to_half(integrated[1]));
            pixels.push_back(float_to_half(0.0F));
            pixels.push_back(float_to_half(1.0F));
        }
    }
    return pixels;
}

[[nodiscard]] std::span<const std::byte>
byte_span(const std::vector<std::uint16_t>& values) noexcept {
    return std::as_bytes(std::span<const std::uint16_t>{values.data(), values.size()});
}

[[nodiscard]] std::vector<graphics::TextureCubeMipDescription>
describe_mips(const std::vector<CubeMipPixels>& storage) {
    std::vector<graphics::TextureCubeMipDescription> descriptions;
    descriptions.reserve(storage.size());
    for (const CubeMipPixels& mip : storage) {
        graphics::TextureCubeMipDescription description;
        description.extent = mip.extent;
        for (std::size_t face = 0; face < description.faces.size(); ++face) {
            description.faces[face] = byte_span(mip.faces[face]);
        }
        descriptions.push_back(description);
    }
    return descriptions;
}

[[nodiscard]] std::uint64_t retained_bytes(const std::vector<CubeMipPixels>& mips,
                                           const std::vector<std::uint16_t>& lut) noexcept {
    std::uint64_t bytes = static_cast<std::uint64_t>(lut.size()) * sizeof(std::uint16_t);
    for (const CubeMipPixels& mip : mips) {
        for (const std::vector<std::uint16_t>& face : mip.faces) {
            bytes += static_cast<std::uint64_t>(face.size()) * sizeof(std::uint16_t);
        }
    }
    return bytes;
}

} // namespace

Result<bool> Renderer::ensure_environment_resources() {
    if (environment_ != nullptr) {
        return false;
    }

    const bool generate_integrated_values = device_->backend() != GraphicsBackend::none;
    std::vector<CubeMipPixels> diffuse_storage;
    diffuse_storage.push_back(generate_integrated_values ? generate_diffuse_irradiance()
                                                         : generate_empty_mip(diffuse_extent));
    std::vector<graphics::TextureCubeMipDescription> diffuse_mips = describe_mips(diffuse_storage);
    const graphics::TextureCubeDescription diffuse_description{
        graphics::TextureFormat::rgba16_float, diffuse_mips, graphics::TextureFilterMode::linear,
        graphics::TextureFilterMode::linear};
    Result<std::unique_ptr<graphics::TextureCube>> diffuse_result =
        device_->create_texture_cube(diffuse_description);
    if (!diffuse_result) {
        return diffuse_result.error();
    }

    std::vector<CubeMipPixels> specular_storage;
    specular_storage.reserve(specular_mip_count);
    for (std::uint32_t level = 0; level < specular_mip_count; ++level) {
        specular_storage.push_back(generate_integrated_values ? generate_specular_mip(level)
                                                              : generate_empty_mip(std::max(
                                                                    specular_extent >> level, 1U)));
    }
    std::vector<graphics::TextureCubeMipDescription> specular_mips =
        describe_mips(specular_storage);
    const graphics::TextureCubeDescription specular_description{
        graphics::TextureFormat::rgba16_float, specular_mips,
        graphics::TextureFilterMode::linear_mipmap_linear, graphics::TextureFilterMode::linear};
    Result<std::unique_ptr<graphics::TextureCube>> specular_result =
        device_->create_texture_cube(specular_description);
    if (!specular_result) {
        return specular_result.error();
    }

    std::vector<std::uint16_t> brdf_pixels =
        generate_integrated_values
            ? generate_brdf_lut()
            : std::vector<std::uint16_t>(static_cast<std::size_t>(brdf_extent) * brdf_extent * 4U);
    const graphics::Texture2DDescription brdf_description{
        {brdf_extent, brdf_extent},
        graphics::TextureFormat::rgba16_float,
        byte_span(brdf_pixels),
        graphics::TextureAddressMode::clamp_to_edge,
        graphics::TextureAddressMode::clamp_to_edge,
        graphics::TextureFilterMode::linear,
        graphics::TextureFilterMode::linear};
    Result<std::unique_ptr<graphics::Texture2D>> brdf_result =
        device_->create_texture_2d(brdf_description);
    if (!brdf_result) {
        return brdf_result.error();
    }

    auto resources = std::make_unique<EnvironmentResources>();
    resources->diffuse = std::move(diffuse_result).value();
    resources->specular = std::move(specular_result).value();
    resources->brdf_lut = std::move(brdf_result).value();
    resources->resident_bytes =
        retained_bytes(diffuse_storage, brdf_pixels) + retained_bytes(specular_storage, {});
    constexpr std::uint64_t maximum_environment_bytes = 2U * 1024U * 1024U;
    if (resources->resident_bytes > maximum_environment_bytes) {
        return Error{ErrorCode::size_overflow,
                     "The built-in studio environment exceeds its 2 MiB residency budget"};
    }
    environment_ = std::move(resources);
    return true;
}

} // namespace elf3d::renderer
