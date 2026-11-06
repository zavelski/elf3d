#pragma once

#include <elf3d/elf3d.h>

#include <imgui.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "viewer_appearance.h"
#include "viewer_document_browser.hpp"
#include "viewer_tools.hpp"

namespace elf3d::viewer {

struct LoadFailure {
    std::string source_path;
    Error error;
};

struct RetainedViewportFrameKey {
    SceneId scene;
    EntityId camera;
    Extent2D target_extent;
    std::uint64_t scene_revision = 0;
    std::uint64_t viewport_revision = 0;
    int diagnostic_render_scale_percent = 100;
    RenderShadingMode shading_mode = RenderShadingMode::standard;
    SelectionSnapshot selection;
    SelectionToolSettings selection_settings;
    std::optional<EntityId> isolated_entity;
    DistanceMeasurementState measurement_state = DistanceMeasurementState::empty;
    std::optional<MeasurementPoint> first_measurement_point;
    std::optional<MeasurementPoint> second_measurement_point;
    std::optional<MeasurementPoint> preview_measurement_point;
    DistanceMeasurementSettings measurement_settings;
    bool operator==(const RetainedViewportFrameKey&) const = default;
};

enum class ViewerHelpTopic : std::uint8_t { shortcuts, model_limits, diagnostics };

struct ViewerShellState {
    std::string window_title;
    bool show_3d_view = true;
    bool show_scene_hierarchy = true;
    bool show_model_information = true;
    bool show_rendering_panel = true;
    bool show_selection_panel = true;
    bool expand_selection_settings = false;
    bool show_measurement_panel = true;
    bool show_navigation_settings = false;
    bool show_status_bar = true;
    bool show_about = false;
    std::optional<ViewerHelpTopic> help_topic;
    bool reset_dock_layout = false;
    bool layout_read_only = false;
    bool dock_layout_initialized = false;
    bool apply_dock_layout = false;
    ImGuiID dock_center_id = 0;
    ImGuiID dock_right_id = 0;
    ImGuiID dock_right_bottom_id = 0;
    float main_menu_height = 0.0F;
    float toolbar_height = 0.0F;
};

struct ViewerRenderingState {
    SceneId graphics_scene;
    GraphicsPreparationBudget graphics_budget;
    SceneGraphicsPreparationReport graphics_report;
    std::optional<Error> graphics_error;
    std::int64_t service_microseconds = 2000;
    Extent2D view_dimensions;
    Extent2D render_target_dimensions;
    bool framebuffer_valid = false;
    std::array<float, 4> clear_color{213.0F / 255.0F, 227.0F / 255.0F, 240.0F / 255.0F, 1.0F};
    BasicLighting lighting;
    EnvironmentLighting environment_lighting;
    DisplayTransform display_transform;
    RenderStatistics statistics;
    RenderShadingMode shading_mode = RenderShadingMode::standard;
    bool backface_culling_enabled = true;
    int diagnostic_render_scale_percent = 100;
    bool vsync_enabled = true;
    bool vsync_applied = true;
    std::optional<RetainedViewportFrameKey> retained_viewport_frame;
    bool viewport_rendered_this_frame = false;
};

struct ViewerFrameSample {
    double frame_milliseconds = 0.0;
    double event_input_milliseconds = 0.0;
    double navigation_scene_milliseconds = 0.0;
    double render_milliseconds = 0.0;
    double ui_composition_milliseconds = 0.0;
    double swap_wait_milliseconds = 0.0;
    double input_to_present_proxy_milliseconds = 0.0;
    RenderStatistics render;
    PickingStatistics picking;
    Extent2D window_dimensions;
    Extent2D framebuffer_dimensions;
    Extent2D view_dimensions;
    Extent2D target_dimensions;
    int render_scale_percent = 100;
    bool vsync_enabled = true;
    bool standard_shading = true;
    bool rendered_3d = false;
};

struct ViewerPerformanceState {
    std::vector<ViewerFrameSample> frame_samples;
    std::uint64_t captured_frame_count = 0;
    std::uint64_t rendered_3d_frame_count = 0;
    std::uint64_t reused_3d_frame_count = 0;
    std::uint64_t sampled_picking_gpu_requests = 0;
    std::uint64_t sampled_picking_cpu_fallbacks = 0;
};

struct ViewerGraphicsDiagnosticsState {
    std::string gl_vendor;
    std::string gl_renderer;
    std::string gl_version;
    std::string glsl_version_report;
    int gl_context_flags = 0;
    int gl_context_profile_mask = 0;
    int default_red_bits = 0;
    int default_green_bits = 0;
    int default_blue_bits = 0;
    int default_alpha_bits = 0;
    int default_depth_bits = 0;
    int default_stencil_bits = 0;
    int default_samples = 0;
    int default_srgb_capable = 0;
    int maximum_texture_size = 0;
};

struct ViewerNotificationState {
    bool request_error_modal = false;
    bool request_save_error_modal = false;
    std::string viewport_error;
    std::optional<LoadFailure> load_failure;
    std::optional<LoadFailure> save_failure;
};

struct ViewerInteractionFrameState {
    bool navigation_active = false;
    bool application_focused = true;
    double frame_delta_seconds = 0.0;
    bool escape_pressed = false;
    bool primary_double_clicked = false;
};

struct SceneHierarchyComponentState {
    DocumentBrowserState browser;
};

struct ViewerPresentationResources {
    std::optional<ImGuiStyle> base_style;
    std::optional<ViewerAppearance> applied_appearance;
    ImFont* main_font = nullptr;
    ImFont* panel_title_font = nullptr;
    ImFont* panel_content_font = nullptr;
};

struct PendingFileInputState {
    std::optional<std::string> dropped_path;
    bool drop_copy_failed = false;
};

struct ViewerFrameContext {
    ViewerShellState& shell;
    ViewerRenderingState& rendering;
    ViewerPerformanceState& performance;
    ViewerGraphicsDiagnosticsState& diagnostics;
    ViewerNotificationState& notifications;
    ViewerInteractionFrameState& interaction;
    SceneHierarchyComponentState& hierarchy;
    ViewerPresentationResources& presentation;
    PendingFileInputState& pending_files;
};

} // namespace elf3d::viewer
