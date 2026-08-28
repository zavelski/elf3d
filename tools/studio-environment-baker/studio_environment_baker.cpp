#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr float pi = 3.14159265359F;
constexpr std::uint32_t environment_integration_sample_count = 1'024;
constexpr std::uint32_t brdf_integration_sample_count = 256;
constexpr std::uint32_t energy_calibration_extent = 256;
constexpr std::uint32_t diffuse_extent = 32;
constexpr std::uint32_t diffuse_mip_count = 1;
constexpr std::uint32_t specular_extent = 128;
constexpr std::uint32_t specular_mip_count = 8;
constexpr std::uint32_t brdf_extent = 256;
constexpr std::uint32_t cubemap_face_count = 6;
constexpr std::uint32_t rgba16_float_format = 1;
constexpr std::uint32_t resource_version = 3;
constexpr std::uint32_t resource_header_bytes = 64;
constexpr std::uint64_t expected_payload_bytes = 1'622'000;
constexpr std::uint64_t expected_brdf_checksum = 0x1FB0C9B7416A7625ULL;
constexpr float maximum_energy_error = 0.01F;
constexpr std::array<std::byte, 8> resource_magic{
    std::byte{'E'}, std::byte{'L'}, std::byte{'F'}, std::byte{'3'},
    std::byte{'D'}, std::byte{'I'}, std::byte{'B'}, std::byte{'L'},
};

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

struct LegacyStudioDescription final {
    Vector3 zenith;
    Vector3 horizon;
    Vector3 ground;
    std::array<StudioLobe, 3> lobes;
};

struct StudioSoftbox final {
    Vector3 direction;
    Vector3 up_hint;
    Vector3 radiance;
    float horizontal_half_angle_radians = 0.0F;
    float vertical_half_angle_radians = 0.0F;
    float relative_edge_softness = 0.0F;
};

struct HighContrastStudioDescription final {
    Vector3 zenith;
    Vector3 horizon;
    Vector3 ground;
    std::array<StudioSoftbox, 4> softboxes;
};

struct StudioEnergyCalibration final {
    Vector3 legacy_mean;
    Vector3 unscaled_mean;
    Vector3 scale;
};

constexpr LegacyStudioDescription legacy_studio{
    {0.18F, 0.19F, 0.21F},
    {0.36F, 0.35F, 0.34F},
    {0.10F, 0.095F, 0.09F},
    {{{{-0.53F, 0.58F, 0.62F}, {3.2F, 3.05F, 2.85F}, 18.0F},
      {{0.70F, 0.22F, 0.68F}, {1.15F, 1.22F, 1.32F}, 10.0F},
      {{0.05F, 0.68F, -0.73F}, {1.75F, 1.70F, 1.62F}, 26.0F}}}};

constexpr HighContrastStudioDescription high_contrast_studio{{0.16F, 0.17F, 0.19F},
                                                             {0.29F, 0.285F, 0.28F},
                                                             {0.09F, 0.085F, 0.08F},
                                                             {{{{-0.53F, 0.58F, 0.62F},
                                                                {0.0F, 1.0F, 0.0F},
                                                                {24.0F, 23.0F, 21.6F},
                                                                0.13962634F,
                                                                0.15707963F,
                                                                0.12F},
                                                               {{0.70F, 0.22F, 0.68F},
                                                                {0.0F, 1.0F, 0.0F},
                                                                {16.0F, 17.2F, 18.8F},
                                                                0.03490659F,
                                                                0.24434610F,
                                                                0.18F},
                                                               {{0.05F, 0.68F, -0.73F},
                                                                {0.0F, 1.0F, 0.0F},
                                                                {15.0F, 14.6F, 14.0F},
                                                                0.17453293F,
                                                                0.05235988F,
                                                                0.20F},
                                                               {{-0.35F, 0.20F, -0.91F},
                                                                {0.0F, 1.0F, 0.0F},
                                                                {10.0F, 11.0F, 12.0F},
                                                                0.07853982F,
                                                                0.08726646F,
                                                                0.16F}}}};

struct CubeMipPixels final {
    std::uint32_t extent = 0;
    std::array<std::vector<std::uint16_t>, cubemap_face_count> faces;
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

[[nodiscard]] Vector3 divide(Vector3 numerator, Vector3 denominator) noexcept {
    return {numerator.x / denominator.x, numerator.y / denominator.y, numerator.z / denominator.z};
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

[[nodiscard]] float smoothstep(float lower, float upper, float value) noexcept {
    const float amount = std::clamp((value - lower) / (upper - lower), 0.0F, 1.0F);
    return amount * amount * (3.0F - 2.0F * amount);
}

[[nodiscard]] Vector3 legacy_environment_radiance(Vector3 direction) noexcept {
    const Vector3 normalized = normalize(direction);
    const float height = std::clamp(normalized.y, -1.0F, 1.0F);
    Vector3 result = height >= 0.0F ? mix(legacy_studio.horizon, legacy_studio.zenith, height)
                                    : mix(legacy_studio.horizon, legacy_studio.ground, -height);
    for (const StudioLobe& lobe : legacy_studio.lobes) {
        const float response =
            std::pow(std::max(dot(normalized, normalize(lobe.direction)), 0.0F), lobe.exponent);
        result = add(result, multiply(lobe.radiance, response));
    }
    return result;
}

[[nodiscard]] float softbox_response(Vector3 direction, const StudioSoftbox& softbox) noexcept {
    const Vector3 forward = normalize(softbox.direction);
    const Vector3 right = normalize(cross(softbox.up_hint, forward));
    const Vector3 up = normalize(cross(forward, right));
    const float forward_distance = dot(direction, forward);
    if (forward_distance <= 0.0001F) {
        return 0.0F;
    }
    const float horizontal = std::abs(dot(direction, right) / forward_distance) /
                             std::tan(softbox.horizontal_half_angle_radians);
    const float vertical = std::abs(dot(direction, up) / forward_distance) /
                           std::tan(softbox.vertical_half_angle_radians);
    constexpr float rounded_rectangle_power = 8.0F;
    const float distance = std::pow(std::pow(horizontal, rounded_rectangle_power) +
                                        std::pow(vertical, rounded_rectangle_power),
                                    1.0F / rounded_rectangle_power);
    return 1.0F - smoothstep(1.0F, 1.0F + softbox.relative_edge_softness, distance);
}

[[nodiscard]] Vector3 unscaled_environment_radiance(Vector3 direction) noexcept {
    const Vector3 normalized = normalize(direction);
    const float height = std::clamp(normalized.y, -1.0F, 1.0F);
    Vector3 result = height >= 0.0F
                         ? mix(high_contrast_studio.horizon, high_contrast_studio.zenith, height)
                         : mix(high_contrast_studio.horizon, high_contrast_studio.ground, -height);
    for (const StudioSoftbox& softbox : high_contrast_studio.softboxes) {
        result = add(result, multiply(softbox.radiance, softbox_response(normalized, softbox)));
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

[[nodiscard]] float cube_texel_solid_angle_weight(std::uint32_t x, std::uint32_t y,
                                                  std::uint32_t extent) noexcept {
    const float u = (2.0F * (static_cast<float>(x) + 0.5F) / static_cast<float>(extent)) - 1.0F;
    const float v = (2.0F * (static_cast<float>(y) + 0.5F) / static_cast<float>(extent)) - 1.0F;
    return 1.0F / std::pow(1.0F + u * u + v * v, 1.5F);
}

template <typename Radiance>
[[nodiscard]] Vector3 solid_angle_weighted_mean(Radiance&& radiance) noexcept {
    Vector3 accumulated;
    float total_weight = 0.0F;
    for (std::size_t face = 0; face < cubemap_face_count; ++face) {
        for (std::uint32_t y = 0; y < energy_calibration_extent; ++y) {
            for (std::uint32_t x = 0; x < energy_calibration_extent; ++x) {
                const float weight = cube_texel_solid_angle_weight(x, y, energy_calibration_extent);
                accumulated =
                    add(accumulated,
                        multiply(radiance(cube_direction(face, x, y, energy_calibration_extent)),
                                 weight));
                total_weight += weight;
            }
        }
    }
    return multiply(accumulated, 1.0F / total_weight);
}

[[nodiscard]] StudioEnergyCalibration calibrate_studio_energy() noexcept {
    const Vector3 legacy_mean = solid_angle_weighted_mean(legacy_environment_radiance);
    const Vector3 unscaled_mean = solid_angle_weighted_mean(unscaled_environment_radiance);
    return {legacy_mean, unscaled_mean, divide(legacy_mean, unscaled_mean)};
}

[[nodiscard]] Vector3 environment_radiance(Vector3 direction, Vector3 calibration) noexcept {
    return multiply(unscaled_environment_radiance(direction), calibration);
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

[[nodiscard]] CubeMipPixels generate_diffuse_irradiance(Vector3 calibration) {
    CubeMipPixels mip;
    mip.extent = diffuse_extent;
    for (std::size_t face = 0; face < mip.faces.size(); ++face) {
        std::vector<std::uint16_t>& pixels = mip.faces[face];
        pixels.reserve(static_cast<std::size_t>(diffuse_extent) * diffuse_extent * 4U);
        for (std::uint32_t y = 0; y < diffuse_extent; ++y) {
            for (std::uint32_t x = 0; x < diffuse_extent; ++x) {
                const Vector3 normal = cube_direction(face, x, y, diffuse_extent);
                Vector3 accumulated;
                for (std::uint32_t sample_index = 0;
                     sample_index < environment_integration_sample_count; ++sample_index) {
                    const Vector3 direction = cosine_hemisphere(
                        hammersley(sample_index, environment_integration_sample_count), normal);
                    accumulated = add(accumulated, environment_radiance(direction, calibration));
                }
                append_half_color(pixels,
                                  multiply(accumulated, pi / environment_integration_sample_count));
            }
        }
    }
    return mip;
}

[[nodiscard]] Vector3 filtered_specular_radiance(const Vector3& reflection, float roughness,
                                                 Vector3 calibration) noexcept {
    Vector3 accumulated;
    float total_weight = 0.0F;
    for (std::uint32_t sample_index = 0; sample_index < environment_integration_sample_count;
         ++sample_index) {
        const Vector3 half_vector = importance_sample_ggx(
            hammersley(sample_index, environment_integration_sample_count), reflection, roughness);
        const Vector3 light = normalize(
            subtract(multiply(half_vector, 2.0F * dot(reflection, half_vector)), reflection));
        const float weight = std::max(dot(reflection, light), 0.0F);
        if (weight > 0.0F) {
            accumulated =
                add(accumulated, multiply(environment_radiance(light, calibration), weight));
            total_weight += weight;
        }
    }
    return multiply(accumulated, 1.0F / std::max(total_weight, 0.001F));
}

[[nodiscard]] CubeMipPixels generate_specular_mip(std::uint32_t level, Vector3 calibration) {
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
                    append_half_color(pixels, environment_radiance(reflection, calibration));
                    continue;
                }
                append_half_color(pixels,
                                  filtered_specular_radiance(reflection, roughness, calibration));
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
    for (std::uint32_t sample_index = 0; sample_index < brdf_integration_sample_count;
         ++sample_index) {
        const Vector3 half_vector = importance_sample_ggx(
            hammersley(sample_index, brdf_integration_sample_count), normal, roughness);
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
    return {scale / brdf_integration_sample_count, bias / brdf_integration_sample_count};
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

void append_u16(std::vector<std::byte>& destination, std::uint16_t value) {
    destination.push_back(static_cast<std::byte>(value & 0xFFU));
    destination.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::byte>& destination, std::uint32_t value) {
    for (std::uint32_t shift = 0; shift < 32U; shift += 8U) {
        destination.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

void append_u64(std::vector<std::byte>& destination, std::uint64_t value) {
    for (std::uint32_t shift = 0; shift < 64U; shift += 8U) {
        destination.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

void append_pixels(std::vector<std::byte>& destination, std::span<const std::uint16_t> pixels) {
    for (const std::uint16_t value : pixels) {
        append_u16(destination, value);
    }
}

struct BakeResult final {
    std::vector<std::byte> resource;
    StudioEnergyCalibration calibration;
};

[[nodiscard]] std::uint64_t fnv1a64(std::span<const std::byte> bytes) noexcept {
    std::uint64_t value = 14'695'981'039'346'656'037ULL;
    for (const std::byte byte : bytes) {
        value ^= std::to_integer<std::uint8_t>(byte);
        value *= 1'099'511'628'211ULL;
    }
    return value;
}

[[nodiscard]] BakeResult bake_resource() {
    const StudioEnergyCalibration calibration = calibrate_studio_energy();
    const Vector3 calibrated_mean = multiply(calibration.unscaled_mean, calibration.scale);
    const auto energy_is_calibrated = [](float calibrated, float legacy) noexcept {
        return std::abs(calibrated - legacy) <= maximum_energy_error * legacy;
    };
    if (!energy_is_calibrated(calibrated_mean.x, calibration.legacy_mean.x) ||
        !energy_is_calibrated(calibrated_mean.y, calibration.legacy_mean.y) ||
        !energy_is_calibrated(calibrated_mean.z, calibration.legacy_mean.z)) {
        return {};
    }
    const CubeMipPixels diffuse = generate_diffuse_irradiance(calibration.scale);
    std::array<CubeMipPixels, specular_mip_count> specular;
    for (std::uint32_t level = 0; level < specular_mip_count; ++level) {
        specular[level] = generate_specular_mip(level, calibration.scale);
    }
    const std::vector<std::uint16_t> brdf = generate_brdf_lut();

    std::vector<std::byte> payload;
    payload.reserve(static_cast<std::size_t>(expected_payload_bytes));
    for (const std::vector<std::uint16_t>& face : diffuse.faces) {
        append_pixels(payload, face);
    }
    for (const CubeMipPixels& mip : specular) {
        for (const std::vector<std::uint16_t>& face : mip.faces) {
            append_pixels(payload, face);
        }
    }
    append_pixels(payload, brdf);
    const std::size_t brdf_bytes = brdf.size() * sizeof(std::uint16_t);
    if (payload.size() != expected_payload_bytes ||
        fnv1a64(std::span<const std::byte>{payload}.last(brdf_bytes)) != expected_brdf_checksum) {
        return {};
    }

    std::vector<std::byte> resource;
    resource.reserve(resource_header_bytes + payload.size());
    resource.insert(resource.end(), resource_magic.begin(), resource_magic.end());
    append_u32(resource, resource_version);
    append_u32(resource, resource_header_bytes);
    append_u32(resource, rgba16_float_format);
    append_u32(resource, cubemap_face_count);
    append_u32(resource, diffuse_extent);
    append_u32(resource, diffuse_mip_count);
    append_u32(resource, specular_extent);
    append_u32(resource, specular_mip_count);
    append_u32(resource, brdf_extent);
    append_u32(resource, brdf_extent);
    append_u64(resource, payload.size());
    append_u64(resource, fnv1a64(payload));
    resource.insert(resource.end(), payload.begin(), payload.end());
    return {std::move(resource), calibration};
}

[[nodiscard]] bool write_resource(const std::filesystem::path& path,
                                  std::span<const std::byte> resource) {
    std::error_code error;
    if (const std::filesystem::path parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        return false;
    }
    stream.write(reinterpret_cast<const char*>(resource.data()),
                 static_cast<std::streamsize>(resource.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] std::vector<std::byte> read_resource(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        return {};
    }
    const std::streampos end = stream.tellg();
    if (end <= 0 ||
        end > static_cast<std::streamoff>(std::numeric_limits<std::streamsize>::max())) {
        return {};
    }
    std::vector<std::byte> result(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(result.data()),
                static_cast<std::streamsize>(result.size()));
    return stream ? result : std::vector<std::byte>{};
}

void print_usage() {
    std::cerr << "Usage: elf3d_studio_environment_baker --output <path> | --verify <path>\n";
}

void print_calibration(const StudioEnergyCalibration& calibration) {
    std::cout << "Studio energy legacy_mean=" << calibration.legacy_mean.x << ','
              << calibration.legacy_mean.y << ',' << calibration.legacy_mean.z
              << " unscaled_mean=" << calibration.unscaled_mean.x << ','
              << calibration.unscaled_mean.y << ',' << calibration.unscaled_mean.z
              << " scale=" << calibration.scale.x << ',' << calibration.scale.y << ','
              << calibration.scale.z << '\n';
}

} // namespace

int main(int argument_count, char** arguments) {
    if (argument_count != 3) {
        print_usage();
        return 2;
    }
    const std::string_view operation = arguments[1];
    const std::filesystem::path path = arguments[2];
    try {
        BakeResult bake = bake_resource();
        if (bake.resource.size() != resource_header_bytes + expected_payload_bytes) {
            std::cerr << "Studio environment bake produced an invalid byte count\n";
            return 3;
        }
        print_calibration(bake.calibration);
        if (operation == "--output") {
            if (!write_resource(path, bake.resource)) {
                std::cerr << "Could not write studio environment resource\n";
                return 4;
            }
            std::cout << "Wrote " << bake.resource.size() << " bytes to " << path.string() << '\n';
            return 0;
        }
        if (operation == "--verify") {
            const std::vector<std::byte> existing = read_resource(path);
            if (existing != bake.resource) {
                std::cerr << "Studio environment resource differs from the canonical bake\n";
                return 5;
            }
            std::cout << "Verified " << bake.resource.size() << " canonical bytes in "
                      << path.string() << '\n';
            return 0;
        }
    } catch (const std::exception& exception) {
        std::cerr << "Studio environment bake failed: " << exception.what() << '\n';
        return 6;
    } catch (...) {
        std::cerr << "Studio environment bake failed unexpectedly\n";
        return 7;
    }
    print_usage();
    return 2;
}
