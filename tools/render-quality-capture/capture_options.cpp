#include "capture_internal.h"

#include <charconv>
#include <cmath>
#include <iostream>
#include <system_error>
#include <utility>

namespace elf3d::capture_tool {

struct Presence final {
    bool model = false;
    bool extent = false;
    bool camera = false;
    bool output = false;
    bool metadata = false;
};

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value)
{
    const auto* begin = reinterpret_cast<const char8_t*>(value.data());
    return std::filesystem::path{std::u8string{begin, begin + value.size()}};
}

[[nodiscard]] std::optional<float> float_value(std::string_view value) noexcept
{
    float parsed = 0.0F;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() &&
                   std::isfinite(parsed)
               ? std::optional<float>{parsed}
               : std::nullopt;
}

[[nodiscard]] std::optional<std::uint32_t> unsigned_value(std::string_view value) noexcept
{
    std::uint32_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size()
               ? std::optional<std::uint32_t>{parsed}
               : std::nullopt;
}

[[nodiscard]] std::optional<elf3d::Extent2D> extent_value(std::string_view value) noexcept
{
    const std::size_t separator = value.find('x');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    const auto width = unsigned_value(value.substr(0, separator));
    const auto height = unsigned_value(value.substr(separator + 1));
    return width && height && *width != 0 && *height != 0
               ? std::optional<elf3d::Extent2D>{{*width, *height}}
               : std::nullopt;
}

[[nodiscard]] std::optional<elf3d::Float4x4> matrix_value(std::string_view value) noexcept
{
    elf3d::Float4x4 matrix;
    std::size_t begin = 0;
    for (std::size_t index = 0; index < matrix.elements.size(); ++index) {
        const std::size_t end = value.find(',', begin);
        const std::string_view token = value.substr(begin, end - begin);
        const auto parsed = float_value(token);
        if (!parsed || (index + 1 != matrix.elements.size() && end == std::string_view::npos) ||
            (index + 1 == matrix.elements.size() && end != std::string_view::npos)) {
            return std::nullopt;
        }
        matrix.elements[index] = *parsed;
        begin = end == std::string_view::npos ? value.size() : end + 1;
    }
    return matrix;
}

template <typename Value>
[[nodiscard]] bool set_once(Value& destination, bool& present, Value value)
{
    if (present) {
        return false;
    }
    destination = std::move(value);
    present = true;
    return true;
}

[[nodiscard]] bool parse_required(std::string_view name, std::string_view value, Options& options,
                                  Presence& presence)
{
    if (name == "--model") {
        return set_once(options.model, presence.model, path_from_utf8(value));
    }
    if (name == "--extent") {
        const auto extent = extent_value(value);
        return extent && set_once(options.extent, presence.extent, *extent);
    }
    if (name == "--output") {
        return set_once(options.output, presence.output, path_from_utf8(value));
    }
    if (name == "--metadata") {
        return set_once(options.metadata, presence.metadata, path_from_utf8(value));
    }
    if (name == "--camera" && !presence.camera) {
        if (value == "authored-first") {
            options.camera_mode = CameraMode::authored_first;
        } else if (value == "authored-opposite") {
            options.camera_mode = CameraMode::authored_opposite;
        } else if (value == "fit-front") {
            options.camera_mode = CameraMode::fit_front;
        } else if (value == "fit-back") {
            options.camera_mode = CameraMode::fit_back;
        } else if (value == "matrix") {
            options.camera_mode = CameraMode::matrix;
        } else {
            return false;
        }
        options.camera_name = value;
        presence.camera = true;
        return true;
    }
    return false;
}

[[nodiscard]] bool parse_render_setting(std::string_view name, std::string_view value,
                                        Options& options)
{
    const auto number = float_value(value);
    if (name == "--matrix") {
        options.camera_matrix = matrix_value(value);
        return options.camera_matrix.has_value();
    }
    if (name == "--shading") {
        if (value == "standard") {
            options.shading = elf3d::RenderShadingMode::standard;
            return true;
        }
        if (value == "unlit") {
            options.shading = elf3d::RenderShadingMode::unlit;
            return true;
        }
        return false;
    }
    if (name == "--tone") {
        if (value == "standard") {
            options.display.tone_mapping = elf3d::ToneMappingMode::standard;
            return true;
        }
        if (value == "pbr-neutral") {
            options.display.tone_mapping = elf3d::ToneMappingMode::pbr_neutral;
            return true;
        }
        if (value == "none") {
            options.display.tone_mapping = elf3d::ToneMappingMode::none;
            return true;
        }
        return false;
    }
    if (name == "--validate-material") {
        if (value == "true") {
            options.validate_material = true;
            return true;
        }
        if (value == "false") {
            options.validate_material = false;
            return true;
        }
        return false;
    }
    if (!number) {
        return false;
    }
    if (name == "--ambient") {
        options.lighting.ambient_intensity = *number;
    } else if (name == "--directional") {
        options.lighting.diffuse_intensity = *number;
    } else if (name == "--environment") {
        options.environment.intensity = *number;
    } else if (name == "--rotation") {
        options.environment.rotation_radians = *number;
    } else if (name == "--exposure") {
        options.display.exposure_ev = *number;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<Options> parse_options(int count, char** values)
{
    Options options;
    Presence presence;
    for (int index = 1; index < count; index += 2) {
        if (index + 1 >= count || values[index] == nullptr || values[index + 1] == nullptr) {
            return std::nullopt;
        }

        const std::string_view name{values[index]};
        const std::string_view value{values[index + 1]};
        if (!parse_required(name, value, options, presence) &&
            !parse_render_setting(name, value, options)) {
            return std::nullopt;
        }
    }
    if (!presence.model || !presence.extent || !presence.camera || !presence.output ||
        !presence.metadata ||
        (options.camera_mode == CameraMode::matrix) != options.camera_matrix.has_value()) {
        return std::nullopt;
    }
    return options;
}

void print_usage()
{
    std::cerr << "Usage: elf3d_render_quality_capture --model <path|procedural-material> "
                 "--extent <width>x<height> "
                 "--camera authored-first|authored-opposite|fit-front|fit-back|matrix "
                 "[--matrix <16-column-major-values>] --output <png> --metadata <json> "
                 "[--shading standard|unlit] [--ambient <value>] [--directional <value>] "
                 "[--environment <value>] [--rotation <radians>] [--exposure <ev>] "
                 "[--tone pbr-neutral|none] [--validate-material true|false]\n";
}

} // namespace elf3d::capture_tool
