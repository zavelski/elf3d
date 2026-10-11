#include "json_document.h"

#include "exporter_json_internal.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <span>
#include <system_error>
#include <utility>

namespace elf3d::gltf::json {
namespace {

[[nodiscard]] bool exact_number(const Value& value, double expected) noexcept
{
    if (value.kind != Kind::number) {
        return false;
    }
    double number = 0;
    const auto parsed =
        std::from_chars(value.token.data(), value.token.data() + value.token.size(), number);
    return parsed.ec == std::errc{} && parsed.ptr == value.token.data() + value.token.size() &&
           number == expected;
}

[[nodiscard]] bool exact_array(const Document& document, std::size_t value,
                               std::span<const double> expected) noexcept
{
    const Value& array = document.values[value];
    if (array.kind != Kind::array || array.children.size() != expected.size()) {
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (!exact_number(document.values[array.children[index]], expected[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool same_texcoord(const Document& document, std::size_t object, std::size_t property)
{
    const auto base = member(document, object, "texCoord");
    const auto selected = unsigned_value(document, property);
    const auto original = base ? unsigned_value(document, *base) : Result<std::size_t>{0U};
    return selected && original && selected.value() == original.value();
}

[[nodiscard]] bool identity_texture_property(const Document& document, std::size_t object,
                                             std::size_t property)
{
    const Value& value = document.values[property];
    if (value.key == "offset") {
        constexpr std::array<double, 2> expected{0, 0};
        return exact_array(document, property, expected);
    }
    if (value.key == "scale") {
        constexpr std::array<double, 2> expected{1, 1};
        return exact_array(document, property, expected);
    }
    if (value.key == "rotation") {
        return exact_number(value, 0);
    }
    return value.key == "texCoord" && same_texcoord(document, object, property);
}

void omit_texture_transform(Document& document, std::size_t object)
{
    const auto extensions = member(document, object, "extensions");
    if (!extensions) {
        return;
    }
    const auto transform = member(document, *extensions, "KHR_texture_transform");
    if (!transform) {
        return;
    }
    const bool identity = std::all_of(
        document.values[*transform].children.begin(), document.values[*transform].children.end(),
        [&document, object](std::size_t property) {
            return identity_texture_property(document, object, property);
        });
    if (identity) {
        erase(document, *extensions, "KHR_texture_transform");
        if (document.values[*extensions].children.empty()) {
            erase(document, object, "extensions");
        }
    }
}

} // namespace

namespace {

void omit_array_default(Document& document, std::size_t object, std::string_view key,
                        std::span<const double> expected)
{
    const auto value = member(document, object, key);
    if (value && exact_array(document, *value, expected)) {
        erase(document, object, key);
    }
}

void omit_node_defaults(Document& document)
{
    const auto nodes = member(document, 0U, "nodes");
    if (!nodes) {
        return;
    }
    constexpr std::array<double, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    constexpr std::array<double, 3> zero{0, 0, 0};
    constexpr std::array<double, 3> one{1, 1, 1};
    constexpr std::array<double, 4> rotation{0, 0, 0, 1};
    for (const std::size_t node : document.values[*nodes].children) {
        omit_array_default(document, node, "matrix", identity);
        omit_array_default(document, node, "translation", zero);
        omit_array_default(document, node, "scale", one);
        omit_array_default(document, node, "rotation", rotation);
    }
}

void queue_core_children(const Document& document, std::size_t object,
                         std::vector<std::size_t>& pending)
{
    for (const std::size_t child : document.values[object].children) {
        const std::string& key = document.values[child].key;
        if (key == "extensions") {
            if (const auto volume = member(document, child, "KHR_materials_volume")) {
                pending.push_back(*volume);
            }
        } else if (key != "extras") {
            pending.push_back(child);
        }
    }
}

} // namespace

void omit_defaults(Document& document)
{
    omit_node_defaults(document);
    // Do not interpret anything inside opaque extras or non-transform extensions.
    std::vector<std::size_t> pending{0U};
    while (!pending.empty()) {
        const std::size_t object = pending.back();
        pending.pop_back();
        omit_texture_transform(document, object);
        const auto extras = member(document, object, "extras");
        if (extras && document.values[*extras].kind == Kind::object &&
            document.values[*extras].children.empty()) {
            erase(document, object, "extras");
        }
        queue_core_children(document, object, pending);
    }
}

void order_sections(Document& document)
{
    constexpr std::array<std::string_view, 17> order{
        "asset",      "extensionsUsed", "extensionsRequired",
        "extensions", "extras",         "scene",
        "scenes",     "buffers",        "bufferViews",
        "accessors",  "images",         "samplers",
        "textures",   "materials",      "meshes",
        "cameras",    "nodes"};
    auto& children = document.values[0].children;
    std::stable_sort(children.begin(), children.end(),
                     [&document, &order](std::size_t left, std::size_t right) {
                         const auto rank = [&order](std::string_view key) {
                             return std::find(order.begin(), order.end(), key) - order.begin();
                         };
                         return rank(document.values[left].key) < rank(document.values[right].key);
                     });
}

namespace {

void prune_extension_list(Document& document, std::string_view key,
                          const std::vector<std::string>& used)
{
    const auto list = member(document, 0U, key);
    if (!list) {
        return;
    }
    auto& children = document.values[*list].children;
    children.erase(std::remove_if(children.begin(), children.end(),
                                  [&document, &used](std::size_t value) {
                                      const auto name = string_value(document.values[value].token);
                                      return !name || std::find(used.begin(), used.end(),
                                                                name.value()) == used.end();
                                  }),
                   children.end());
    if (children.empty()) {
        erase(document, 0U, key);
    }
}

} // namespace

void reconcile_extensions(Document& document)
{
    std::vector<std::string> used;
    std::vector<std::size_t> pending{0U};
    while (!pending.empty()) {
        const std::size_t object = pending.back();
        pending.pop_back();
        if (const auto extensions = member(document, object, "extensions")) {
            for (const std::size_t extension : document.values[*extensions].children) {
                const std::string& name = document.values[extension].key;
                if (std::find(used.begin(), used.end(), name) == used.end()) {
                    used.push_back(name);
                }
            }
        }
        queue_core_children(document, object, pending);
    }
    prune_extension_list(document, "extensionsUsed", used);
    prune_extension_list(document, "extensionsRequired", used);
}

namespace {

[[nodiscard]] bool orthonormal_basis(const std::array<float, 16>& matrix) noexcept
{
    if (matrix[3] != 0.0F || matrix[7] != 0.0F || matrix[11] != 0.0F || matrix[15] != 1.0F) {
        return false;
    }
    for (std::size_t left = 0; left < 3U; ++left) {
        for (std::size_t right = left; right < 3U; ++right) {
            double dot = 0;
            for (std::size_t row = 0; row < 3U; ++row) {
                dot += static_cast<double>(matrix[left * 4U + row]) * matrix[right * 4U + row];
            }
            const double expected = left == right ? 1.0 : 0.0;
            if (std::abs(dot - expected) > 1.0e-6) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] std::optional<std::array<float, 16>> matrix_value(const Document& document,
                                                                std::size_t value) noexcept
{
    const Value& array = document.values[value];
    if (array.kind != Kind::array || array.children.size() != 16U) {
        return std::nullopt;
    }
    std::array<float, 16> matrix{};
    for (std::size_t index = 0; index < matrix.size(); ++index) {
        const Value& number = document.values[array.children[index]];
        const auto parsed = std::from_chars(
            number.token.data(), number.token.data() + number.token.size(), matrix[index]);
        if (number.kind != Kind::number || parsed.ec != std::errc{} ||
            parsed.ptr != number.token.data() + number.token.size() ||
            !std::isfinite(matrix[index])) {
            return std::nullopt;
        }
    }
    return matrix;
}

[[nodiscard]] std::array<float, 16> rounded_basis(std::array<float, 16> candidate) noexcept
{
    for (std::size_t column_index = 0; column_index < 3U; ++column_index) {
        for (std::size_t row = 0; row < 3U; ++row) {
            float& component = candidate[column_index * 4U + row];
            const float nearest = std::round(component);
            if (std::abs(component - nearest) <= 1.0e-7F) {
                component = nearest;
            }
        }
    }
    return candidate;
}

[[nodiscard]] bool clean_matrix(Document& document, std::size_t value,
                                const std::array<float, 16>& matrix)
{
    const auto candidate = rounded_basis(matrix);
    if (candidate == matrix || !orthonormal_basis(candidate)) {
        return false;
    }
    for (std::size_t index = 0; index < candidate.size(); ++index) {
        if (candidate[index] == matrix[index]) {
            continue;
        }
        std::string token;
        exporter_detail::append_float(token, candidate[index]);
        document.values[document.values[value].children[index]].token = std::move(token);
    }
    return true;
}

} // namespace

std::size_t clean_rotation_matrices(Document& document)
{
    const auto nodes = member(document, 0U, "nodes");
    if (!nodes) {
        return 0U;
    }
    std::size_t cleaned = 0;
    for (const std::size_t node : document.values[*nodes].children) {
        const auto value = member(document, node, "matrix");
        if (!value) {
            continue;
        }
        const auto matrix = matrix_value(document, *value);
        if (!matrix || !orthonormal_basis(*matrix)) {
            continue;
        }
        if (!clean_matrix(document, *value, *matrix)) {
            continue;
        }
        ++cleaned;
    }
    return cleaned;
}

} // namespace elf3d::gltf::json
