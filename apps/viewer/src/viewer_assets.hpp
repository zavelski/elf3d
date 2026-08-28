#pragma once

#include <elf3d/elf3d.h>
#include <elf3d/imgui/texture.h>

#include <imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "viewer_scene_session.hpp"

namespace elf3d::viewer {

struct DecodedImage {
    std::vector<unsigned char> rgba;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

class ToolbarTexture final {
  public:
    ToolbarTexture() noexcept = default;
    ~ToolbarTexture();
    ToolbarTexture(const ToolbarTexture&) = delete;
    ToolbarTexture& operator=(const ToolbarTexture&) = delete;
    ToolbarTexture(ToolbarTexture&& other) noexcept;
    ToolbarTexture& operator=(ToolbarTexture&& other) noexcept;
    [[nodiscard]] bool upload(const DecodedImage& image) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] ImTextureRef texture_ref() const noexcept;

  private:
    std::unique_ptr<imgui::UiTexture> texture_;
};

enum class ToolbarIcon : std::size_t {
    open,
    save_as,
    fit_view,
    reset_camera,
    select,
    measure,
    clipping_panel,
    section_plane,
    add_clipping_box,
    clear_clipping,
    hide_selected,
    show_selected,
    isolate_selected,
    show_all,
    reset_layout,
    count,
};

struct ToolbarIcons {
    std::array<ToolbarTexture, static_cast<std::size_t>(ToolbarIcon::count)> textures;
    [[nodiscard]] const ToolbarTexture& texture(ToolbarIcon icon) const noexcept {
        return textures[static_cast<std::size_t>(icon)];
    }
};

[[nodiscard]] std::filesystem::path viewer_asset_root(int argument_count, char** arguments);
[[nodiscard]] ToolbarIcons load_toolbar_icons(const std::filesystem::path& asset_root);
[[nodiscard]] Result<SceneSession> create_empty_scene(Engine& engine);
[[nodiscard]] Result<SceneSession> load_model_scene(Engine& engine,
                                                    const std::filesystem::path& path);

} // namespace elf3d::viewer
