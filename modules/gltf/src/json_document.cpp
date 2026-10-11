#include "json_document.h"

#include <elf3d/core/error.h>

#include "exporter_json_internal.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <system_error>
#include <utility>

namespace elf3d::gltf::json {
namespace {

[[nodiscard]] Error invalid()
{
    return Error{ErrorCode::malformed_gltf, "Invalid or oversized retained JSON"};
}

[[nodiscard]] bool whitespace(char character) noexcept
{
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

void skip_space(std::string_view source, std::size_t& position) noexcept
{
    while (position < source.size() && whitespace(source[position])) {
        ++position;
    }
}

[[nodiscard]] bool hex_digit(char value) noexcept
{
    return std::string_view{"0123456789abcdefABCDEF"}.find(value) != std::string_view::npos;
}

[[nodiscard]] bool skip_escape(std::string_view source, std::size_t& position) noexcept
{
    if (++position >= source.size()) {
        return false;
    }
    if (source[position] != 'u') {
        return std::string_view{"\"\\/bfnrt"}.find(source[position]) != std::string_view::npos;
    }
    if (source.size() - position <= 4U) {
        return false;
    }
    for (std::size_t digit = 0; digit < 4U; ++digit) {
        if (!hex_digit(source[++position])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Result<std::size_t> string_end(std::string_view source, std::size_t position)
{
    for (++position; position < source.size(); ++position) {
        const char character = source[position];
        if (character == '"') {
            return position + 1U;
        }
        if (static_cast<unsigned char>(character) < 0x20U) {
            return invalid();
        }
        if (character == '\\' && !skip_escape(source, position)) {
            return invalid();
        }
    }
    return invalid();
}

[[nodiscard]] bool digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] bool digits(std::string_view token, std::size_t& position) noexcept
{
    const std::size_t start = position;
    while (position < token.size() && digit(token[position])) {
        ++position;
    }
    return position != start;
}

[[nodiscard]] bool integer_part(std::string_view token, std::size_t& position) noexcept
{
    if (position >= token.size()) {
        return false;
    }
    if (token[position] == '0') {
        ++position;
        return true;
    }
    return token[position] >= '1' && token[position] <= '9' && digits(token, position);
}

[[nodiscard]] bool exponent_part(std::string_view token, std::size_t& position) noexcept
{
    if (position >= token.size() || (token[position] != 'e' && token[position] != 'E')) {
        return true;
    }
    ++position;
    if (position < token.size() && (token[position] == '+' || token[position] == '-')) {
        ++position;
    }
    return digits(token, position);
}

[[nodiscard]] bool number_token(std::string_view token) noexcept
{
    std::size_t position = 0;
    if (position < token.size() && token[position] == '-') {
        ++position;
    }
    if (!integer_part(token, position)) {
        return false;
    }
    if (position < token.size() && token[position] == '.') {
        ++position;
        if (!digits(token, position)) {
            return false;
        }
    }
    return exponent_part(token, position) && position == token.size();
}

enum class State { first, next, separator };

struct ParseFrame {
    std::size_t value = 0;
    State state = State::first;
};

[[nodiscard]] Result<void> read_scalar(Value& value, std::string_view source, std::size_t& position)
{
    const std::size_t begin = position;
    while (position < source.size() && !whitespace(source[position]) &&
           std::string_view{",]}"}.find(source[position]) == std::string_view::npos) {
        ++position;
    }
    const std::string_view token = source.substr(begin, position - begin);
    if (token == "true" || token == "false" || token == "null") {
        value.kind = Kind::literal;
    } else if (number_token(token)) {
        value.kind = Kind::number;
    } else {
        return invalid();
    }
    value.token.assign(token);
    return {};
}

[[nodiscard]] Result<std::size_t> append_value(Document& document, std::string_view source,
                                               std::size_t& position, std::string key)
{
    if (document.values.size() >= maximum_json_values || position >= source.size()) {
        return invalid();
    }
    Value value;
    value.key = std::move(key);
    const char character = source[position];
    if (character == '{' || character == '[') {
        value.kind = character == '{' ? Kind::object : Kind::array;
        ++position;
    } else if (character == '"') {
        const auto end = string_end(source, position);
        if (!end) {
            return end.error();
        }
        value.kind = Kind::string;
        value.token.assign(source.substr(position, end.value() - position));
        position = end.value();
    } else {
        const auto scalar = read_scalar(value, source, position);
        if (!scalar) {
            return scalar.error();
        }
    }
    const std::size_t index = document.values.size();
    document.values.push_back(std::move(value));
    return index;
}

[[nodiscard]] bool container(Kind kind) noexcept
{
    return kind == Kind::object || kind == Kind::array;
}

[[nodiscard]] Result<std::string> parse_key(std::string_view source, std::size_t& position)
{
    if (position >= source.size() || source[position] != '"') {
        return invalid();
    }
    const auto end = string_end(source, position);
    if (!end) {
        return end.error();
    }
    auto result = string_value(source.substr(position, end.value() - position));
    position = end.value();
    skip_space(source, position);
    if (position >= source.size() || source[position++] != ':') {
        return invalid();
    }
    return result;
}

void line_break(std::string& output, std::size_t depth)
{
    output.push_back('\n');
    output.append(depth, '\t');
}

[[nodiscard]] std::size_t column(std::string_view output) noexcept
{
    const std::size_t newline = output.rfind('\n');
    std::size_t result = 0;
    for (const char character :
         output.substr(newline == std::string_view::npos ? 0U : newline + 1U)) {
        result = character == '\t' ? (result / 4U + 1U) * 4U : result + 1U;
    }
    return result;
}

[[nodiscard]] bool compact_array(const Document& document, const Value& value,
                                 std::size_t current_column) noexcept
{
    if (value.kind != Kind::array || value.key == "matrix") {
        return false;
    }
    std::size_t width = 2U;
    for (const std::size_t child : value.children) {
        if (container(document.values[child].kind)) {
            return false;
        }
        width += document.values[child].token.size() + 2U;
        if (current_column > 140U || width > 140U - current_column) {
            return false;
        }
    }
    return true;
}

struct WriteFrame {
    std::size_t value = 0;
    std::size_t next = 0;
    std::size_t depth = 0;
    bool entered = false;
};

[[nodiscard]] Result<std::uint32_t> code_unit(std::string_view token, std::size_t& position)
{
    if (token.size() - position < 6U) {
        return invalid();
    }
    std::uint32_t value = 0;
    const auto parsed =
        std::from_chars(token.data() + position + 1U, token.data() + position + 5U, value, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + position + 5U) {
        return invalid();
    }
    position += 4U;
    return value;
}

[[nodiscard]] Result<std::uint32_t> surrogate_pair(std::string_view token, std::size_t& position,
                                                   std::uint32_t high)
{
    if (token.size() - position < 8U || token[position + 1U] != '\\' ||
        token[position + 2U] != 'u') {
        return invalid();
    }
    position += 2U;
    const auto low = code_unit(token, position);
    if (!low || low.value() < 0xdc00U || low.value() > 0xdfffU) {
        return invalid();
    }
    return 0x10000U + ((high - 0xd800U) << 10U) + low.value() - 0xdc00U;
}

[[nodiscard]] Result<std::uint32_t> unicode_point(std::string_view token, std::size_t& position)
{
    const auto unit = code_unit(token, position);
    if (!unit) {
        return unit.error();
    }
    const std::uint32_t point = unit.value();
    if (point < 0xd800U || point > 0xdfffU) {
        return point;
    }
    if (point > 0xdbffU) {
        return invalid();
    }
    return surrogate_pair(token, position, point);
}

void append_utf8(std::string& result, std::uint32_t point)
{
    // Every emitted byte is bounded to the unsigned-char range.
    const auto byte = [&result](std::uint32_t value) {
        result.push_back(static_cast<char>(static_cast<unsigned char>(value)));
    };
    if (point < 0x80U) {
        byte(point);
    } else if (point < 0x800U) {
        byte(0xc0U | (point >> 6U));
        byte(0x80U | (point & 0x3fU));
    } else if (point < 0x10000U) {
        byte(0xe0U | (point >> 12U));
        byte(0x80U | ((point >> 6U) & 0x3fU));
        byte(0x80U | (point & 0x3fU));
    } else {
        byte(0xf0U | (point >> 18U));
        byte(0x80U | ((point >> 12U) & 0x3fU));
        byte(0x80U | ((point >> 6U) & 0x3fU));
        byte(0x80U | (point & 0x3fU));
    }
}

[[nodiscard]] Result<std::string> object_key(const Document& document, std::size_t parent,
                                             std::string_view source, std::size_t& position)
{
    if (document.values[parent].kind != Kind::object) {
        return std::string{};
    }
    auto key = parse_key(source, position);
    if (!key) {
        return key.error();
    }
    if (member(document, parent, key.value())) {
        return Error{ErrorCode::malformed_gltf, "Duplicate JSON object member"};
    }
    return key;
}

[[nodiscard]] Result<void> parse_child(Document& document, std::vector<ParseFrame>& stack,
                                       std::string_view source, std::size_t& position)
{
    const std::size_t parent = stack.back().value;
    auto key = object_key(document, parent, source, position);
    if (!key) {
        return key.error();
    }
    skip_space(source, position);
    const auto child = append_value(document, source, position, std::move(key).value());
    if (!child) {
        return child.error();
    }
    document.values[parent].children.push_back(child.value());
    stack.back().state = State::separator;
    if (!container(document.values[child.value()].kind)) {
        return {};
    }
    if (stack.size() >= maximum_json_depth) {
        return invalid();
    }
    stack.push_back({child.value(), State::first});
    return {};
}

[[nodiscard]] Result<void> parse_step(Document& document, std::vector<ParseFrame>& stack,
                                      std::string_view source, std::size_t& position)
{
    skip_space(source, position);
    if (position >= source.size()) {
        return invalid();
    }
    const std::size_t parent = stack.back().value;
    const char closing = document.values[parent].kind == Kind::object ? '}' : ']';
    if (source[position] == closing && stack.back().state != State::next) {
        ++position;
        stack.pop_back();
        return {};
    }
    if (stack.back().state != State::separator) {
        return parse_child(document, stack, source, position);
    }
    if (source[position++] != ',') {
        return invalid();
    }
    stack.back().state = State::next;
    return {};
}

} // namespace

Result<std::string> string_value(std::string_view token)
{
    if (token.size() < 2U || token.front() != '"' || token.back() != '"') {
        return invalid();
    }
    std::string result;
    for (std::size_t position = 1; position + 1U < token.size(); ++position) {
        if (token[position] != '\\') {
            result.push_back(token[position]);
            continue;
        }
        if (++position + 1U >= token.size()) {
            return invalid();
        }
        const char escape = token[position];
        if (escape != 'u') {
            constexpr std::string_view escaped = "\"\\/bfnrt";
            constexpr std::array<char, 8> decoded{'"', '\\', '/', '\b', '\f', '\n', '\r', '\t'};
            const std::size_t index = escaped.find(escape);
            if (index == std::string_view::npos) {
                return invalid();
            }
            result.push_back(decoded[index]);
            continue;
        }
        const auto point = unicode_point(token, position);
        if (!point) {
            return point.error();
        }
        append_utf8(result, point.value());
    }
    return result;
}

Result<Document> parse(std::string_view source)
{
    if (source.empty() || source.size() > maximum_json_bytes) {
        return invalid();
    }
    Document document;
    std::size_t position = 0;
    skip_space(source, position);
    const auto root = append_value(document, source, position, {});
    if (!root) {
        return root.error();
    }
    std::vector<ParseFrame> stack;
    if (container(document.values[0].kind)) {
        stack.push_back({0U, State::first});
    }
    while (!stack.empty()) {
        const auto step = parse_step(document, stack, source, position);
        if (!step) {
            return step.error();
        }
    }
    skip_space(source, position);
    if (position != source.size()) {
        return invalid();
    }
    return document;
}

std::optional<std::size_t> member(const Document& document, std::size_t object,
                                  std::string_view key) noexcept
{
    for (const std::size_t value : document.values[object].children) {
        if (document.values[value].key == key) {
            return value;
        }
    }
    return std::nullopt;
}

void erase(Document& document, std::size_t object, std::string_view key)
{
    auto& children = document.values[object].children;
    children.erase(
        std::remove_if(children.begin(), children.end(),
                       [&document, key](std::size_t id) { return document.values[id].key == key; }),
        children.end());
}

Result<std::size_t> unsigned_value(const Document& document, std::size_t value)
{
    const auto& token = document.values[value];
    std::size_t result = 0;
    const auto parsed =
        std::from_chars(token.token.data(), token.token.data() + token.token.size(), result);
    if (token.kind != Kind::number || parsed.ec != std::errc{} ||
        parsed.ptr != token.token.data() + token.token.size()) {
        return invalid();
    }
    return result;
}

namespace {

[[nodiscard]] bool enter_value(const Document& document, WriteFrame& frame, std::string& output)
{
    const Value& value = document.values[frame.value];
    if (!container(value.kind)) {
        output.append(value.token);
        return true;
    }
    output.push_back(value.kind == Kind::object ? '{' : '[');
    if (!value.children.empty() && !compact_array(document, value, column(output) - 1U)) {
        frame.entered = true;
        return false;
    }
    for (std::size_t index = 0; index < value.children.size(); ++index) {
        if (index != 0U) {
            output.append(", ");
        }
        output.append(document.values[value.children[index]].token);
    }
    output.push_back(value.kind == Kind::object ? '}' : ']');
    return true;
}

void write_child(const Document& document, std::vector<WriteFrame>& stack, std::string& output)
{
    WriteFrame& frame = stack.back();
    const Value& value = document.values[frame.value];
    const std::size_t child = value.children[frame.next];
    if (frame.next != 0U) {
        output.push_back(',');
    }
    const bool matrix_group = value.kind == Kind::array && value.key == "matrix" &&
                              value.children.size() == 16U && frame.next % 4U != 0U &&
                              column(output) + document.values[child].token.size() + 2U <= 140U;
    if (matrix_group) {
        output.push_back(' ');
    } else {
        line_break(output, frame.depth + 1U);
    }
    if (value.kind == Kind::object) {
        exporter_detail::append_string(output, document.values[child].key);
        output.append(": ");
    }
    ++frame.next;
    const std::size_t depth = frame.depth + 1U;
    stack.push_back({child, 0U, depth, false});
}

void write_step(const Document& document, std::vector<WriteFrame>& stack, std::string& output)
{
    WriteFrame& frame = stack.back();
    if (!frame.entered && enter_value(document, frame, output)) {
        stack.pop_back();
        return;
    }
    const Value& value = document.values[frame.value];
    if (frame.next != value.children.size()) {
        write_child(document, stack, output);
        return;
    }
    line_break(output, frame.depth);
    output.push_back(value.kind == Kind::object ? '}' : ']');
    stack.pop_back();
}

} // namespace

Result<std::string> write(const Document& document, std::size_t root)
{
    if (root >= document.values.size()) {
        return invalid();
    }
    std::string output;
    std::vector<WriteFrame> stack{{root, 0U, 0U, false}};
    while (!stack.empty()) {
        write_step(document, stack, output);
        if (stack.size() > maximum_json_depth || output.size() > maximum_json_bytes * 4U) {
            return invalid();
        }
    }
    output.push_back('\n');
    return output;
}

} // namespace elf3d::gltf::json
