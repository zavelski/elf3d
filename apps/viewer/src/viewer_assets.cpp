#include "viewer_internal.hpp"

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

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
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

template <typename TextureId> TextureId to_imgui_texture_id(std::uintptr_t value) noexcept {
    if constexpr (std::is_pointer_v<TextureId>) {
        return reinterpret_cast<TextureId>(value);
    } else {
        return static_cast<TextureId>(value);
    }
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
        reset();
        texture_ = std::exchange(other.texture_, 0U);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

bool ToolbarTexture::upload(const DecodedImage& image) noexcept {
    if (image.rgba.empty() || image.width == 0 || image.height == 0) {
        return false;
    }
    reset();
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(image.width),
                 static_cast<GLsizei>(image.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image.rgba.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    texture_ = texture;
    width_ = static_cast<int>(image.width);
    height_ = static_cast<int>(image.height);
    return true;
}

void ToolbarTexture::reset() noexcept {
    if (texture_ != 0U) {
        glDeleteTextures(1, &texture_);
        texture_ = 0U;
    }
    width_ = 0;
    height_ = 0;
}

bool ToolbarTexture::is_valid() const noexcept {
    return texture_ != 0U && width_ > 0 && height_ > 0;
}

ImTextureRef ToolbarTexture::texture_ref() const noexcept {
    return ImTextureRef{to_imgui_texture_id<ImTextureID>(static_cast<std::uintptr_t>(texture_))};
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

elf3d::Quaternion axis_angle(elf3d::Float3 axis, float radians) noexcept {
    const float half_angle = radians * 0.5F;
    const float sine = std::sin(half_angle);
    return elf3d::Quaternion{axis.x * sine, axis.y * sine, axis.z * sine, std::cos(half_angle)};
}

[[nodiscard]] elf3d::Result<elf3d::EntityId> create_viewer_camera(elf3d::Scene& scene) {
    const elf3d::Result<elf3d::EntityId> camera_result =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
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

[[nodiscard]] elf3d::Result<ViewerScene> create_demo_scene(elf3d::Engine& engine) {
    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = engine.create_scene();
    if (!scene_result) {
        return scene_result.error();
    }
    std::unique_ptr<elf3d::Scene> scene = std::move(scene_result).value();

    constexpr float low = -0.5F;
    constexpr float high = 0.5F;
    const std::array<elf3d::VertexPositionNormal, 24> vertices{{
        {{low, low, high}, {0.0F, 0.0F, 1.0F}},   {{high, low, high}, {0.0F, 0.0F, 1.0F}},
        {{high, high, high}, {0.0F, 0.0F, 1.0F}}, {{low, high, high}, {0.0F, 0.0F, 1.0F}},
        {{high, low, low}, {0.0F, 0.0F, -1.0F}},  {{low, low, low}, {0.0F, 0.0F, -1.0F}},
        {{low, high, low}, {0.0F, 0.0F, -1.0F}},  {{high, high, low}, {0.0F, 0.0F, -1.0F}},
        {{high, low, high}, {1.0F, 0.0F, 0.0F}},  {{high, low, low}, {1.0F, 0.0F, 0.0F}},
        {{high, high, low}, {1.0F, 0.0F, 0.0F}},  {{high, high, high}, {1.0F, 0.0F, 0.0F}},
        {{low, low, low}, {-1.0F, 0.0F, 0.0F}},   {{low, low, high}, {-1.0F, 0.0F, 0.0F}},
        {{low, high, high}, {-1.0F, 0.0F, 0.0F}}, {{low, high, low}, {-1.0F, 0.0F, 0.0F}},
        {{low, high, high}, {0.0F, 1.0F, 0.0F}},  {{high, high, high}, {0.0F, 1.0F, 0.0F}},
        {{high, high, low}, {0.0F, 1.0F, 0.0F}},  {{low, high, low}, {0.0F, 1.0F, 0.0F}},
        {{low, low, low}, {0.0F, -1.0F, 0.0F}},   {{high, low, low}, {0.0F, -1.0F, 0.0F}},
        {{high, low, high}, {0.0F, -1.0F, 0.0F}}, {{low, low, high}, {0.0F, -1.0F, 0.0F}},
    }};
    const std::array<std::uint32_t, 36> indices{{
        0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    }};

    const elf3d::Result<elf3d::MeshHandle> mesh_result =
        scene->create_mesh(elf3d::MeshDataView{vertices, indices});
    if (!mesh_result) {
        return mesh_result.error();
    }
    const elf3d::Result<elf3d::MaterialHandle> material_result = scene->create_material(
        elf3d::MaterialDescription{elf3d::Color4{0.72F, 0.32F, 0.12F, 1.0F}});
    if (!material_result) {
        return material_result.error();
    }
    const elf3d::Result<elf3d::EntityId> cube_result =
        scene->create_model(mesh_result.value(), material_result.value());
    if (!cube_result) {
        return cube_result.error();
    }

    const elf3d::Result<elf3d::EntityId> camera_result = create_viewer_camera(*scene);
    if (!camera_result) {
        return camera_result.error();
    }
    const std::optional<elf3d::Bounds3> bounds = scene->world_bounds();

    return ViewerScene{std::move(scene),
                       camera_result.value(),
                       cube_result.value(),
                       material_result.value(),
                       {},
                       elf3d::SceneStatistics{2, 1, 1, 1, 1, 24, 36, 12},
                       bounds,
                       true};
}

[[nodiscard]] elf3d::Result<ViewerScene> load_model_scene(elf3d::Engine& engine,
                                                          const std::filesystem::path& path) {
    elf3d::Result<elf3d::LoadedScene> loaded_result =
        engine.load_scene_with_report(path_to_utf8(path));
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

    ViewerScene result{std::move(scene),
                       camera_result.value(),
                       std::nullopt,
                       std::nullopt,
                       path,
                       source_statistics,
                       bounds,
                       true};
    result.load_report = std::move(loaded.report);
    return result;
}

void glfw_error_callback(int error_code, const char* description) {
    std::cerr << "GLFW error " << error_code << ": "
              << (description != nullptr ? description : "No description") << '\n';
}

void glfw_drop_callback(GLFWwindow* window, int path_count, const char** paths) {
    auto* state = static_cast<ViewerState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr || path_count <= 0 || paths == nullptr || paths[0] == nullptr) {
        return;
    }
    try {
        state->dropped_path = paths[0];
    } catch (const std::bad_alloc&) {
        fatal_viewer_allocation_failure();
    } catch (const std::length_error&) {
        state->drop_copy_failed = true;
    } catch (...) {
        fatal_unexpected_viewer_exception();
    }
}

void glfw_navigation_scroll_callback(GLFWwindow* window, double, double y_offset) {
    auto* state = static_cast<ViewerState*>(glfwGetWindowUserPointer(window));
    const float delta = static_cast<float>(y_offset);
    if (state == nullptr || !std::isfinite(delta) || delta == 0.0F) {
        return;
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    const ImVec2 position{static_cast<float>(x), static_cast<float>(y)};
    const float accumulated_delta = state->navigation_wheel_delta + delta;
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(accumulated_delta)) {
        return;
    }

    state->navigation_wheel_delta = accumulated_delta;
    state->navigation_wheel_position = position;
}

} // namespace elf3d::viewer
