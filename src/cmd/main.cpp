#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "arguments.h"
#include "diagnostics.h"
#include "rotate_document.h"

#include <elf3d/core/assert.h>
#include <elf3d/model.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] std::filesystem::path native_path(std::string_view utf8)
{
    std::u8string value;
    for (const unsigned char byte : utf8) {
        value.push_back(static_cast<char8_t>(byte));
    }
    return std::filesystem::path{value};
}

[[nodiscard]] elf3d::Result<void> check_distinct_paths(const elf3d::cmd::Arguments& arguments)
{
    const auto input = native_path(arguments.input);
    const auto output = native_path(arguments.output);
    std::error_code error;
    const auto normalized_input = std::filesystem::weakly_canonical(input, error);
    if (error) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Cannot normalize input path"};
    }
    const auto normalized_output = std::filesystem::weakly_canonical(output, error);
    if (error) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Cannot normalize output path"};
    }
    const bool same_file = std::filesystem::equivalent(input, output, error);
    // equivalence can fail for a destination that does not yet exist.
    if (normalized_input == normalized_output || (!error && same_file)) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Input and output must be distinct files"};
    }
    return {};
}

[[nodiscard]] int process(std::span<const std::string_view> tokens)
{
    const auto arguments = elf3d::cmd::parse_arguments(tokens);
    if (!arguments) {
        elf3d::cmd::print_error("arguments", arguments.error());
        elf3d::cmd::print_usage();
        return 2;
    }
    if (arguments.value().help) {
        elf3d::cmd::print_usage();
        return 0;
    }
    const auto distinct = check_distinct_paths(arguments.value());
    if (!distinct) {
        elf3d::cmd::print_error("arguments", distinct.error());
        return 2;
    }

    auto loaded = elf3d::load_document(arguments.value().input);
    if (!loaded) {
        elf3d::cmd::print_error("load", loaded.error());
        return 1;
    }
    elf3d::cmd::print_report(loaded.value().report);
    elf3d::Document& document = loaded.value().document;
    const auto rotated = elf3d::cmd::rotate_document(document, arguments.value().rotation_degrees);
    if (!rotated) {
        elf3d::cmd::print_error("transform", rotated.error());
        return 1;
    }
    const auto validation = elf3d::validate_document(document.view());
    elf3d::cmd::print_report(validation);
    if (validation.has_errors()) {
        return 1;
    }
    elf3d::ModelWriteOptions write_options;
    write_options.numeric_policy = elf3d::ModelNumericWritePolicy::clean_rotation;
    const auto saved =
        elf3d::save_document(arguments.value().output, document.view(), write_options);
    if (!saved) {
        elf3d::cmd::print_error("save", saved.error());
        return 1;
    }
    elf3d::cmd::print_report(saved.value());
    std::cout << "Saved " << arguments.value().output << " (" << document.statistics().nodes
              << " nodes, " << document.statistics().triangles << " triangles)\n";
    return 0;
}

#if defined(_WIN32)
[[nodiscard]] elf3d::Result<std::string> argument_utf8(std::wstring_view value)
{
    if (value.size() > std::numeric_limits<int>::max()) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Argument exceeds Windows conversion limit"};
    }
    // The length check proves that Windows' signed length can represent this view.
    const int length = static_cast<int>(value.size());
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length,
                                         nullptr, 0, nullptr, nullptr);
    if (size == 0) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Cannot convert argument to UTF-8"};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length, result.data(),
                            size, nullptr, nullptr) != size) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Cannot convert argument to UTF-8"};
    }
    return result;
}
#endif

} // namespace

#if defined(_WIN32)
int wmain(int count, wchar_t** arguments)
#else
int main(int count, char** arguments)
#endif
{
    try {
        std::vector<std::string> owned;
        for (int index = 1; index < count; ++index) {
#if defined(_WIN32)
            auto converted = argument_utf8(arguments[index]);
            if (!converted) {
                elf3d::cmd::print_error("arguments", converted.error());
                return 2;
            }
            owned.push_back(std::move(converted).value());
#else
            owned.emplace_back(arguments[index]);
#endif
        }
        std::vector<std::string_view> tokens;
        for (const std::string& argument : owned) {
            tokens.emplace_back(argument);
        }
        return process(tokens);
    } catch (const std::bad_alloc&) {
        elf3d::fatal_error("Cmd argument or diagnostic allocation failed");
    } catch (const std::system_error& error) {
        elf3d::cmd::print_error("arguments",
                                elf3d::Error{elf3d::ErrorCode::invalid_argument, error.what()});
        return 2;
    } catch (...) {
        elf3d::fatal_error("Unexpected failure at Cmd process boundary");
    }
}
