#include "arguments.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <system_error>

namespace elf3d::cmd {
namespace {

[[nodiscard]] bool supported_path(std::string_view path) noexcept
{
    return !path.empty() && path.find('\0') == std::string_view::npos &&
           (path.ends_with(".gltf") || path.ends_with(".glb"));
}

[[nodiscard]] Result<float> parse_angle(std::string_view token) noexcept
{
    float coordinate = 0.0F;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), coordinate);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
        !std::isfinite(coordinate)) {
        return Error{ErrorCode::invalid_argument, "Rotation angles must be finite float values"};
    }
    return coordinate;
}

[[nodiscard]] Result<Float3> parse_rotation(std::span<const std::string_view> tokens,
                                            bool repeated) noexcept
{
    if (repeated || tokens.size() < 4U) {
        return Error{ErrorCode::invalid_argument,
                     "--rotate requires exactly three angles in degrees, once"};
    }
    std::array<float, 3> coordinates{};
    for (std::size_t index = 0; index < coordinates.size(); ++index) {
        const auto parsed = parse_angle(tokens[index + 1U]);
        if (!parsed) {
            return parsed.error();
        }
        coordinates[index] = parsed.value();
    }
    return Float3{coordinates[0], coordinates[1], coordinates[2]};
}

[[nodiscard]] Result<void> assign_path(Arguments& result, std::string_view option,
                                       std::string_view token)
{
    if (option != "--input" && option != "--output") {
        return Error{ErrorCode::invalid_argument, "Unknown option; use --help for usage"};
    }
    std::string& path = option == "--input" ? result.input : result.output;
    if (!path.empty() || !supported_path(token)) {
        return Error{ErrorCode::invalid_argument,
                     "Input/output must occur once and name a .gltf or .glb file"};
    }
    path.assign(token);
    return {};
}

[[nodiscard]] bool complete(const Arguments& result, bool rotated) noexcept
{
    return !result.input.empty() && !result.output.empty() && rotated;
}

} // namespace

Result<Arguments> parse_arguments(std::span<const std::string_view> tokens)
{
    Arguments result;
    if (tokens.size() == 1U && tokens.front() == "--help") {
        result.help = true;
        return result;
    }
    bool rotated = false;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const auto option = tokens[index];
        if (option == "--rotate") {
            const auto angles = parse_rotation(tokens.subspan(index), rotated);
            if (!angles) {
                return angles.error();
            }
            result.rotation_degrees = angles.value();
            rotated = true;
            index += 3U;
        } else {
            if (index + 1U >= tokens.size()) {
                return Error{ErrorCode::invalid_argument,
                             "Option requires a value; use --help for usage"};
            }
            const auto assigned = assign_path(result, option, tokens[++index]);
            if (!assigned) {
                return assigned.error();
            }
        }
    }
    if (!complete(result, rotated)) {
        return Error{ErrorCode::invalid_argument, "--input, --output, and --rotate are required"};
    }
    return result;
}

} // namespace elf3d::cmd
