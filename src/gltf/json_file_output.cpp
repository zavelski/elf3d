#include "json_document.h"
#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d::gltf::json {
namespace {
[[nodiscard]] std::vector<std::size_t> entries(const Document& document, std::string_view name)
{
    const auto table = member(document, 0, name);
    return table ? document.values[*table].children : std::vector<std::size_t>{};
}
[[nodiscard]] std::string unique_name(std::string text, std::size_t index,
                                      const std::set<std::string>& reserved,
                                      const std::set<std::string>& used)
{
    const bool fallback = text.empty();
    if (fallback) {
        text = "Mesh_" + std::to_string(index + 1);
    }
    if (used.contains(text) || (fallback && reserved.contains(text))) {
        const auto base = text;
        std::size_t suffix = 0;
        do {
            text = base + "-" + std::to_string(suffix++);
        } while (reserved.contains(text) || used.contains(text));
    }
    return text;
}

void set_mesh_name(Document& document, std::size_t mesh, std::string_view original,
                   std::string_view text)
{
    const auto name = member(document, mesh, "name");
    std::string token = name ? document.values[*name].token : "\"\"";
    if (original.empty()) {
        token = "\"" + std::string{text} + "\"";
    } else {
        token.insert(token.size() - 1, text.substr(original.size()));
    }
    const auto next = document.values.size();
    document.values.push_back({Kind::string, "name", std::move(token), {}});
    if (name) {
        auto& children = document.values[mesh].children;
        *std::find(children.begin(), children.end(), *name) = next;
    } else {
        document.values[mesh].children.push_back(next);
    }
}

[[nodiscard]] Result<void> mesh_names(Document& document)
{
    std::set<std::string> reserved, used;
    const auto meshes = entries(document, "meshes");
    for (const auto mesh : meshes) {
        if (const auto name = member(document, mesh, "name")) {
            const auto text = string_value(document.values[*name].token);
            if (!text) {
                return text.error();
            }
            reserved.insert(text.value());
        }
    }
    for (std::size_t index = 0; index < meshes.size(); ++index) {
        const auto mesh = meshes[index];
        auto name = member(document, mesh, "name");
        const auto original =
            name ? string_value(document.values[*name].token) : Result<std::string>{std::string{}};
        if (!original) {
            return original.error();
        }
        const auto text = unique_name(original.value(), index, reserved, used);
        used.insert(text);
        if (text != original.value()) {
            set_mesh_name(document, mesh, original.value(), text);
        }
    }
    return {};
}
} // namespace
Result<void> prepare_file_output(Document& document)
{
    return mesh_names(document);
}
} // namespace elf3d::gltf::json
