#include "viewer_assets.hpp"

#include "viewer_application.hpp"
#include "viewer_browser.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <objbase.h>
#include <wincodec.h>
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elf3d::viewer {

[[nodiscard]] std::filesystem::path executable_directory(int argument_count, char** arguments) {
    if (argument_count > 0 && arguments != nullptr && arguments[0] != nullptr) {
        std::error_code error;
        const std::filesystem::path executable =
            std::filesystem::absolute(path_from_utf8(arguments[0]), error);
        if (!error && executable.has_parent_path()) {
            return executable.parent_path();
        }
    }

    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    return error ? std::filesystem::path{"."} : current;
}

[[nodiscard]] std::filesystem::path viewer_asset_root(int argument_count, char** arguments) {
    const std::filesystem::path executable_assets =
        executable_directory(argument_count, arguments) / "assets";
    std::error_code error;
    if (std::filesystem::exists(executable_assets, error)) {
        return executable_assets;
    }

    const std::filesystem::path source_assets =
        std::filesystem::current_path(error) / "apps" / "viewer" / "assets";
    if (!error && std::filesystem::exists(source_assets, error)) {
        return source_assets;
    }
    return executable_assets;
}

#if defined(_WIN32)
template <typename T> class ComPtr final {
  public:
    ComPtr() noexcept = default;
    ~ComPtr() {
        reset();
    }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    [[nodiscard]] T* get() const noexcept {
        return pointer_;
    }

    [[nodiscard]] T** put() noexcept {
        reset();
        return &pointer_;
    }

    [[nodiscard]] T* operator->() const noexcept {
        return pointer_;
    }

    void reset() noexcept {
        if (pointer_ != nullptr) {
            pointer_->Release();
            pointer_ = nullptr;
        }
    }

  private:
    T* pointer_ = nullptr;
};

class ComInitialization final {
  public:
    ComInitialization() noexcept : result_{CoInitializeEx(nullptr, COINIT_MULTITHREADED)} {}

    ~ComInitialization() {
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool can_use_com() const noexcept {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

  private:
    HRESULT result_;
};

struct WicDecodeState {
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    UINT width = 0;
    UINT height = 0;
};

[[nodiscard]] bool initialize_wic_factory(WicDecodeState& state) noexcept {
    return SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(state.factory.put())));
}

[[nodiscard]] bool decode_wic_frame(WicDecodeState& state,
                                    const std::filesystem::path& path) noexcept {
    if (FAILED(state.factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                        WICDecodeMetadataCacheOnLoad,
                                                        state.decoder.put()))) {
        return false;
    }
    if (FAILED(state.decoder->GetFrame(0, state.frame.put()))) {
        return false;
    }
    return SUCCEEDED(state.frame->GetSize(&state.width, &state.height)) && state.width != 0 &&
           state.height != 0;
}

[[nodiscard]] bool initialize_wic_converter(WicDecodeState& state) noexcept {
    if (FAILED(state.factory->CreateFormatConverter(state.converter.put()))) {
        return false;
    }
    return SUCCEEDED(state.converter->Initialize(state.frame.get(), GUID_WICPixelFormat32bppRGBA,
                                                 WICBitmapDitherTypeNone, nullptr, 0.0,
                                                 WICBitmapPaletteTypeCustom));
}

[[nodiscard]] std::optional<DecodedImage> copy_wic_pixels(WicDecodeState& state) noexcept {
    const std::size_t byte_count =
        static_cast<std::size_t>(state.width) * static_cast<std::size_t>(state.height) * 4U;
    if (byte_count > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
        return std::nullopt;
    }
    DecodedImage decoded;
    decoded.width = state.width;
    decoded.height = state.height;
    decoded.rgba.resize(byte_count);
    const UINT stride = state.width * 4U;
    if (FAILED(state.converter->CopyPixels(nullptr, stride, static_cast<UINT>(decoded.rgba.size()),
                                           decoded.rgba.data()))) {
        return std::nullopt;
    }
    return decoded;
}
#endif

[[nodiscard]] std::optional<DecodedImage>
decode_png_rgba(const std::filesystem::path& path) noexcept {
#if defined(_WIN32)
    const ComInitialization com;
    if (!com.can_use_com()) {
        return std::nullopt;
    }
    WicDecodeState state;
    if (!initialize_wic_factory(state)) {
        return std::nullopt;
    }
    if (!decode_wic_frame(state, path)) {
        return std::nullopt;
    }
    if (!initialize_wic_converter(state)) {
        return std::nullopt;
    }
    return copy_wic_pixels(state);
#else
    (void)path;
    return std::nullopt;
#endif
}

ToolbarTexture::~ToolbarTexture() {
    reset();
}

ToolbarTexture::ToolbarTexture(ToolbarTexture&& other) noexcept {
    *this = std::move(other);
}

ToolbarTexture& ToolbarTexture::operator=(ToolbarTexture&& other) noexcept {
    if (this != &other) {
        texture_ = std::move(other.texture_);
    }
    return *this;
}

bool ToolbarTexture::upload(const DecodedImage& image) noexcept {
    if (image.rgba.empty() || image.width == 0 || image.height == 0) {
        return false;
    }
    const std::span<const unsigned char> pixels{image.rgba};
    elf3d::Result<std::unique_ptr<elf3d::imgui::UiTexture>> created =
        elf3d::imgui::UiTexture::create(elf3d::imgui::UiTextureDescription{
            std::as_bytes(pixels), elf3d::Extent2D{image.width, image.height}});
    if (!created) {
        return false;
    }
    texture_ = std::move(created).value();
    return texture_->is_valid();
}

void ToolbarTexture::reset() noexcept {
    texture_.reset();
}

bool ToolbarTexture::is_valid() const noexcept {
    return texture_ != nullptr && texture_->is_valid();
}

ImTextureRef ToolbarTexture::texture_ref() const noexcept {
    return texture_ != nullptr ? texture_->texture_ref() : ImTextureRef{};
}

struct ToolbarIconSpec {
    ToolbarIcon icon;
    const char* file_name;
};

constexpr std::array<ToolbarIconSpec, static_cast<std::size_t>(ToolbarIcon::count)>
    toolbar_icon_specs{{
        {ToolbarIcon::open, "open.png"},
        {ToolbarIcon::save_as, "save_as.png"},
        {ToolbarIcon::fit_view, "fit_view.png"},
        {ToolbarIcon::reset_camera, "reset_camera.png"},
        {ToolbarIcon::select, "select.png"},
        {ToolbarIcon::measure, "measure.png"},
        {ToolbarIcon::clipping_panel, "clipping_panel.png"},
        {ToolbarIcon::section_plane, "section_plane.png"},
        {ToolbarIcon::add_clipping_box, "add_clipping_box.png"},
        {ToolbarIcon::clear_clipping, "clear_clipping.png"},
        {ToolbarIcon::hide_selected, "hide_selected.png"},
        {ToolbarIcon::show_selected, "show_selected.png"},
        {ToolbarIcon::isolate_selected, "isolate_selected.png"},
        {ToolbarIcon::show_all, "show_all.png"},
        {ToolbarIcon::reset_layout, "reset_layout.png"},
    }};

[[nodiscard]] ToolbarIcons load_toolbar_icons(const std::filesystem::path& asset_root) {
    ToolbarIcons icons;
    const std::filesystem::path icon_root = asset_root / "icon";
    for (const ToolbarIconSpec& spec : toolbar_icon_specs) {
        std::optional<DecodedImage> image = decode_png_rgba(icon_root / spec.file_name);
        if (image.has_value()) {
            const bool uploaded =
                icons.textures[static_cast<std::size_t>(spec.icon)].upload(*image);
            (void)uploaded;
        }
    }
    return icons;
}

[[nodiscard]] elf3d::Result<elf3d::EntityId> create_viewer_camera(elf3d::Scene& scene) {
    const elf3d::Result<elf3d::EntityId> camera_result =
        scene.create_perspective_camera_entity(elf3d::PerspectiveCameraDescription{});
    if (!camera_result) {
        return camera_result.error();
    }
    elf3d::Transform transform;
    transform.translation = {0.0F, 0.0F, 3.0F};
    const elf3d::Result<void> transform_result =
        scene.set_local_transform(camera_result.value(), transform);
    if (!transform_result) {
        return transform_result.error();
    }
    return camera_result.value();
}

[[nodiscard]] elf3d::Result<SceneSession> create_empty_scene(elf3d::Engine& engine) {
    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = engine.create_scene();
    if (!scene_result) {
        return scene_result.error();
    }
    std::unique_ptr<elf3d::Scene> scene = std::move(scene_result).value();

    const elf3d::Result<elf3d::EntityId> camera_result = create_viewer_camera(*scene);
    if (!camera_result) {
        return camera_result.error();
    }
    return SceneSession{std::move(scene), camera_result.value(), {}, {}, std::nullopt, false};
}

[[nodiscard]] elf3d::Result<SceneSession> load_model_scene(elf3d::Engine& engine,
                                                           const std::filesystem::path& path) {
    elf3d::Result<elf3d::LoadedScene> loaded_result = engine.load_scene(path_to_utf8(path));
    if (!loaded_result) {
        return loaded_result.error();
    }
    elf3d::LoadedScene loaded = std::move(loaded_result).value();
    std::unique_ptr<elf3d::Scene> scene = std::move(loaded.scene);
    const elf3d::SceneStatistics source_statistics = scene->statistics();
    const std::optional<elf3d::Bounds3> bounds = scene->world_bounds();
    const elf3d::Result<elf3d::EntityId> camera_result = create_viewer_camera(*scene);
    if (!camera_result) {
        return camera_result.error();
    }

    SceneSession result{
        std::move(scene), camera_result.value(), path, source_statistics, bounds, true};
    result.load_report = std::move(loaded.report);
    return result;
}

} // namespace elf3d::viewer
