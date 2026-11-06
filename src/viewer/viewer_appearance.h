#ifndef ELF3D_VIEWER_APPEARANCE_H
#define ELF3D_VIEWER_APPEARANCE_H

#include <cstdint>

namespace elf3d::viewer {
enum class ViewerTheme : std::uint8_t { light, dark };
struct ViewerAppearance {
    ViewerTheme theme = ViewerTheme::light;
    std::uint16_t ui_scale_percent = 100;
    std::uint16_t text_scale_percent = 100;
    bool operator==(const ViewerAppearance&) const = default;
};
[[nodiscard]] constexpr bool valid_viewer_appearance(ViewerAppearance value) noexcept
{
    return (value.theme == ViewerTheme::light || value.theme == ViewerTheme::dark) &&
           value.ui_scale_percent >= 100 && value.ui_scale_percent <= 200 &&
           value.text_scale_percent >= 75 && value.text_scale_percent <= 150;
}
struct ViewerAssembly;
struct ViewerPreferencesState;
void apply_viewer_appearance(ViewerAssembly& runtime) noexcept;
void build_viewer_appearance_menu(ViewerPreferencesState& preferences);
} // namespace elf3d::viewer
#endif
