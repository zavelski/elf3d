#include "viewer_appearance.h"
#include "viewer_browser.hpp"
#include "viewer_runtime_state.hpp"

#include <imgui.h>

#include <array>
#include <span>
#include <string>

namespace elf3d::viewer {
namespace {
[[nodiscard]] bool theme_menu(ViewerAppearance& appearance)
{
    if (!ImGui::BeginMenu("Theme"))
        return false;
    const auto previous = appearance.theme;
    if (ImGui::MenuItem("Light", nullptr, appearance.theme == ViewerTheme::light))
        appearance.theme = ViewerTheme::light;
    if (ImGui::MenuItem("Dark", nullptr, appearance.theme == ViewerTheme::dark))
        appearance.theme = ViewerTheme::dark;
    ImGui::EndMenu();
    return previous != appearance.theme;
}
[[nodiscard]] bool scale_menu(const char* title, std::uint16_t& percent,
                              std::span<const std::uint16_t> choices)
{
    if (!ImGui::BeginMenu(title))
        return false;
    const auto previous = percent;
    for (const auto choice : choices) {
        const auto label = std::to_string(choice) + "%";
        if (ImGui::MenuItem(label.c_str(), nullptr, percent == choice))
            percent = choice;
    }
    ImGui::EndMenu();
    return previous != percent;
}
} // namespace
void apply_viewer_appearance(ViewerAssembly& runtime) noexcept
{
    auto& presentation = runtime.presentation;
    const auto desired = runtime.preferences.appearance;
    if (!valid_viewer_appearance(desired))
        return;
    if (!presentation.base_style) {
        presentation.base_style = ImGui::GetStyle();
    }
    if (presentation.applied_appearance == desired)
        return;
    auto style = *presentation.base_style;
    if (desired.theme == ViewerTheme::dark)
        ImGui::StyleColorsDark(&style);
    const float scale = static_cast<float>(desired.ui_scale_percent) / 100;
    style.ScaleAllSizes(scale);
    style.FontScaleMain *= scale * static_cast<float>(desired.text_scale_percent) / 100;
    // Monitor font scaling remains integration-owned and is not a user preference.
    style.FontScaleDpi = ImGui::GetStyle().FontScaleDpi;
    ImGui::GetStyle() = style;
    presentation.applied_appearance = desired;
}
void build_viewer_appearance_menu(ViewerPreferencesState& preferences)
{
    auto& appearance = preferences.appearance;
    bool changed = theme_menu(appearance);
    constexpr std::array<std::uint16_t, 5> interface_scales{100, 125, 150, 175, 200};
    constexpr std::array<std::uint16_t, 5> text_scales{75, 90, 100, 125, 150};
    changed = scale_menu("UI Scale", appearance.ui_scale_percent, interface_scales) || changed;
    if (ImGui::BeginMenu("Fonts")) {
        ImGui::TextDisabled("DroidSans");
        changed = scale_menu("Text Size", appearance.text_scale_percent, text_scales) || changed;
        ImGui::EndMenu();
    }
    if (changed)
        mark_viewer_preferences_changed(preferences);
}
} // namespace elf3d::viewer
