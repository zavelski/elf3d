#pragma once

#include <elf3d/elf3d.h>

#include <imgui.h>

namespace elf3d::viewer {

inline constexpr float viewer_ui_font_size_pixels = 20.0F;
inline constexpr float panel_content_font_size_pixels = viewer_ui_font_size_pixels * 0.70F;
inline constexpr float panel_title_font_size_pixels = viewer_ui_font_size_pixels * 0.875F;
inline constexpr float side_dock_width_fraction = 0.27968F;

class ScopedFont final {
  public:
    explicit ScopedFont(ImFont* font) noexcept : font_(font) {
        if (font_ != nullptr) {
            ImGui::PushFont(font_);
        }
    }

    ~ScopedFont() {
        if (font_ != nullptr) {
            ImGui::PopFont();
        }
    }

    ScopedFont(const ScopedFont&) = delete;
    ScopedFont& operator=(const ScopedFont&) = delete;

  private:
    ImFont* font_ = nullptr;
};

[[nodiscard]] inline bool begin_panel_window(const char* name, bool* open, ImFont* title_font,
                                             ImGuiWindowFlags flags = ImGuiWindowFlags_None) {
    const ScopedFont title_scope{title_font};
    return ImGui::Begin(name, open, flags);
}

void set_default_dock(ImGuiID dock_id, bool force);
void tooltip(const char* text);
[[nodiscard]] const char* error_category(ErrorCode code) noexcept;
void push_professional_dialog_style();
void pop_professional_dialog_style();

} // namespace elf3d::viewer
