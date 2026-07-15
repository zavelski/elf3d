module;

#include <elf3d/core/assert.h>

#include "exporter_json_internal.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

module elf.gltf;

namespace elf3d::gltf::exporter_detail {
namespace {

[[nodiscard]] bool is_json_whitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

void append_string_token(std::string& output, std::string_view source, std::size_t& index) {
    const std::size_t start = index;
    bool escaped = false;
    while (index < source.size()) {
        const char value = source[index];
        output.push_back(value);
        if (index != start && value == '"' && !escaped) {
            return;
        }
        escaped = value == '\\' && !escaped;
        ++index;
    }
}

[[nodiscard]] std::optional<std::size_t> scalar_array_end(std::string_view source,
                                                          std::size_t start) noexcept {
    bool in_string = false;
    bool escaped = false;
    for (std::size_t index = start + 1U; index < source.size(); ++index) {
        const char value = source[index];
        if (in_string) {
            if (value == '"' && !escaped) {
                in_string = false;
            }
            escaped = value == '\\' && !escaped;
            continue;
        }
        if (value == '"') {
            in_string = true;
            escaped = false;
        } else if (value == '[' || value == '{') {
            return std::nullopt;
        } else if (value == ']') {
            return index;
        }
    }
    return std::nullopt;
}

void append_compact_array(std::string& output, std::string_view source, std::size_t start,
                          std::size_t end) {
    output.push_back('[');
    for (std::size_t index = start + 1U; index < end; ++index) {
        const char value = source[index];
        if (is_json_whitespace(value)) {
            continue;
        }
        if (value == '"') {
            append_string_token(output, source, index);
        } else {
            output.push_back(value);
            if (value == ',') {
                output.push_back(' ');
            }
        }
    }
    output.push_back(']');
}

[[nodiscard]] std::size_t next_json_token(std::string_view source, std::size_t start) noexcept {
    while (start < source.size() && is_json_whitespace(source[start])) {
        ++start;
    }
    return start;
}

void append_line_break(std::string& output, std::size_t depth) {
    output.push_back('\n');
    output.append(depth, '\t');
}

void append_preserved_value(std::string& output, std::string_view source, std::size_t& index) {
    ++index;
    while (index < source.size() && source[index] != preserved_json_end) {
        output.push_back(source[index]);
        ++index;
    }
    ELF3D_ASSERT(index < source.size());
}

void append_array_token(std::string& output, std::string_view source, std::size_t& index,
                        std::size_t& depth) {
    const std::optional<std::size_t> end = scalar_array_end(source, index);
    if (end.has_value()) {
        append_compact_array(output, source, index, *end);
        index = *end;
        return;
    }
    output.push_back('[');
    ++depth;
    append_line_break(output, depth);
}

void append_object_token(std::string& output, std::string_view source, std::size_t& index,
                         std::size_t& depth) {
    const std::size_t next = next_json_token(source, index + 1U);
    if (next < source.size() && source[next] == '}') {
        output.append("{}");
        index = next;
        return;
    }
    output.push_back('{');
    ++depth;
    append_line_break(output, depth);
}

void append_other_token(std::string& output, char value, std::size_t& depth) {
    if (value == ']' || value == '}') {
        ELF3D_ASSERT(depth > 0U);
        --depth;
        append_line_break(output, depth);
        output.push_back(value);
    } else if (value == ',') {
        output.push_back(value);
        append_line_break(output, depth);
    } else if (value == ':') {
        output.append(": ");
    } else {
        output.push_back(value);
    }
}

} // namespace

std::string format_json(std::string_view source) {
    std::string output;
    output.reserve(source.size() + source.size() / 8U);
    std::size_t depth = 0U;
    for (std::size_t index = 0U; index < source.size(); ++index) {
        const char value = source[index];
        if (is_json_whitespace(value)) {
            continue;
        }
        if (value == '"') {
            append_string_token(output, source, index);
            continue;
        }
        if (value == preserved_json_begin) {
            append_preserved_value(output, source, index);
            continue;
        }
        if (value == '[') {
            append_array_token(output, source, index, depth);
            continue;
        }
        if (value == '{') {
            append_object_token(output, source, index, depth);
            continue;
        }
        append_other_token(output, value, depth);
    }
    output.push_back('\n');
    return output;
}

} // namespace elf3d::gltf::exporter_detail
