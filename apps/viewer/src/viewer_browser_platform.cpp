#include "viewer_internal.hpp"

#include <array>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(_WIN32)
#include <shellapi.h>
#endif

namespace elf3d::viewer {

#if defined(_WIN32)

[[nodiscard]] bool regular_file(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

[[nodiscard]] std::optional<std::filesystem::path>
search_executable_path(const wchar_t* executable_name) {
    constexpr DWORD maximum_path_characters = 32768;
    std::vector<wchar_t> buffer(maximum_path_characters, L'\0');
    const DWORD length = SearchPathW(nullptr, executable_name, nullptr, maximum_path_characters,
                                     buffer.data(), nullptr);
    if (length == 0 || length >= maximum_path_characters) {
        return std::nullopt;
    }
    const std::filesystem::path path{buffer.data()};
    return regular_file(path) ? std::optional<std::filesystem::path>{path} : std::nullopt;
}

[[nodiscard]] std::optional<std::filesystem::path>
registry_executable_path(HKEY root, const wchar_t* executable_name, DWORD registry_view) {
    const std::wstring subkey =
        std::wstring{L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"} +
        executable_name;
    constexpr DWORD accepted_types = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ;
    DWORD byte_count = 0;
    DWORD value_type = 0;
    const DWORD flags = accepted_types | registry_view;
    if (RegGetValueW(root, subkey.c_str(), nullptr, flags, &value_type, nullptr, &byte_count) !=
            ERROR_SUCCESS ||
        byte_count < sizeof(wchar_t)) {
        return std::nullopt;
    }

    std::vector<wchar_t> value(byte_count / sizeof(wchar_t) + 1U, L'\0');
    if (RegGetValueW(root, subkey.c_str(), nullptr, flags, &value_type, value.data(),
                     &byte_count) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    std::wstring path_text{value.data()};
    if (path_text.size() >= 2U && path_text.front() == L'"' && path_text.back() == L'"') {
        path_text = path_text.substr(1U, path_text.size() - 2U);
    }
    const std::filesystem::path path{std::move(path_text)};
    return regular_file(path) ? std::optional<std::filesystem::path>{path} : std::nullopt;
}

[[nodiscard]] std::optional<std::filesystem::path>
registered_executable_path(const wchar_t* executable_name) {
#if defined(_WIN64)
    constexpr std::array<DWORD, 2> registry_views{RRF_SUBKEY_WOW6464KEY, RRF_SUBKEY_WOW6432KEY};
#else
    constexpr std::array<DWORD, 1> registry_views{0};
#endif
    const std::array<HKEY, 2> roots{HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
    for (HKEY root : roots) {
        for (DWORD registry_view : registry_views) {
            const std::optional<std::filesystem::path> path =
                registry_executable_path(root, executable_name, registry_view);
            if (path.has_value()) {
                return path;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::filesystem::path>
known_installation_path(const char* environment_name, const std::filesystem::path& relative_path) {
    const std::optional<std::filesystem::path> directory = environment_directory(environment_name);
    if (!directory.has_value()) {
        return std::nullopt;
    }
    const std::filesystem::path candidate = *directory / relative_path;
    return regular_file(candidate) ? std::optional<std::filesystem::path>{candidate} : std::nullopt;
}

[[nodiscard]] std::optional<std::filesystem::path>
find_editor(const wchar_t* executable_name, const std::filesystem::path& program_files_relative,
            const std::filesystem::path& local_app_data_relative = {}) {
    if (std::optional<std::filesystem::path> path = search_executable_path(executable_name);
        path.has_value()) {
        return path;
    }
    if (std::optional<std::filesystem::path> path = registered_executable_path(executable_name);
        path.has_value()) {
        return path;
    }
    if (!program_files_relative.empty()) {
        for (const char* environment_name : {"ProgramFiles", "ProgramFiles(x86)"}) {
            if (std::optional<std::filesystem::path> path =
                    known_installation_path(environment_name, program_files_relative);
                path.has_value()) {
                return path;
            }
        }
    }
    if (!local_app_data_relative.empty()) {
        return known_installation_path("LOCALAPPDATA", local_app_data_relative);
    }
    return std::nullopt;
}

#endif

ExternalEditorPaths find_external_editors() {
#if defined(_WIN32)
    ExternalEditorPaths editors;
    editors.emeditor =
        find_editor(L"EmEditor.exe", "EmEditor/EmEditor.exe", "Programs/EmEditor/EmEditor.exe");
    editors.notepad = find_editor(L"notepad.exe", {});
    editors.notepad_plus_plus = find_editor(L"notepad++.exe", "Notepad++/notepad++.exe",
                                            "Programs/Notepad++/notepad++.exe");
    return editors;
#else
    return {};
#endif
}

bool launch_external_editor(const std::filesystem::path& editor,
                            const std::filesystem::path& file) {
#if defined(_WIN32)
    const std::wstring parameters = L"\"" + file.native() + L"\"";
    SHELLEXECUTEINFOW request{};
    request.cbSize = sizeof(request);
    request.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    request.lpVerb = L"open";
    request.lpFile = editor.c_str();
    request.lpParameters = parameters.c_str();
    request.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&request) != FALSE;
#else
    static_cast<void>(editor);
    static_cast<void>(file);
    return false;
#endif
}

} // namespace elf3d::viewer
