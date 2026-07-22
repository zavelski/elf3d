#include "viewer_internal.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>

namespace elf3d::viewer {
namespace {

[[nodiscard]] constexpr int csv_flag(bool value) noexcept {
    return value ? 1 : 0;
}

void write_csv_header(std::ofstream& stream) {
    stream << "frame,frame_ms,event_input_ms,navigation_scene_ms,render_ms,ui_composition_ms,"
              "swap_wait_ms,input_to_present_proxy_ms,rendered_3d,render_list_ms,resources_ms,"
              "gl_submission_ms,gpu_main_available,gpu_main_ms,gpu_resolve_available,"
              "gpu_resolve_ms,candidates,visible,culled,draw_calls,material_switches,"
              "shader_switches,instanced_draws,passes,buffer_uploads,buffer_upload_bytes,"
              "resident_geometry_bytes,resident_texture_bytes,pick_pass_ms,pick_readback_ms,"
              "pick_allocation_ms,pick_cpu_ms,pick_gpu_available,pick_gpu_ms,pick_draw_calls,"
              "pick_pixels_read,pick_target_allocations,window_width,window_height,"
              "framebuffer_width,framebuffer_height,view_width,view_height,target_width,"
              "target_height,render_scale_percent,vsync,standard_shading,gl_vendor,gl_renderer,"
              "gl_version,glsl_version,context_flags,profile_mask,red_bits,green_bits,blue_bits,"
              "alpha_bits,depth_bits,stencil_bits,samples,framebuffer_srgb_enabled,"
              "max_texture_size\n";
}

void write_csv_row(std::ofstream& stream, std::size_t index, const ViewerState::FrameSample& sample,
                   const ViewerState& state) {
    stream << index << ',' << sample.frame_milliseconds << ',' << sample.event_input_milliseconds
           << ',' << sample.navigation_scene_milliseconds << ',' << sample.render_milliseconds
           << ',' << sample.ui_composition_milliseconds << ',' << sample.swap_wait_milliseconds
           << ',' << sample.input_to_present_proxy_milliseconds << ','
           << csv_flag(sample.rendered_3d) << ',' << sample.render.cpu_render_list_milliseconds
           << ',' << sample.render.cpu_resource_preparation_milliseconds << ','
           << sample.render.cpu_gl_submission_milliseconds << ','
           << csv_flag(sample.render.gpu_main_pass_timing_available) << ','
           << sample.render.gpu_main_pass_milliseconds << ','
           << csv_flag(sample.render.gpu_resolve_timing_available) << ','
           << sample.render.gpu_resolve_milliseconds << ',' << sample.render.candidate_primitives
           << ',' << sample.render.visible_primitives << ','
           << sample.render.frustum_culled_primitives << ',' << sample.render.draw_calls << ','
           << sample.render.material_switches << ',' << sample.render.shader_switches << ','
           << sample.render.instanced_draw_calls << ',' << sample.render.render_passes << ','
           << sample.render.gpu_buffer_uploads << ',' << sample.render.gpu_buffer_uploaded_bytes
           << ',' << sample.render.estimated_resident_geometry_bytes << ','
           << sample.render.estimated_resident_texture_bytes << ','
           << sample.picking.latest_pass_milliseconds << ','
           << sample.picking.latest_readback_milliseconds << ','
           << sample.picking.latest_allocation_milliseconds << ','
           << sample.picking.latest_cpu_milliseconds << ','
           << csv_flag(sample.picking.latest_gpu_timing_available) << ','
           << sample.picking.latest_gpu_milliseconds << ',' << sample.picking.latest_gpu_draw_calls
           << ',' << sample.picking.latest_gpu_pixels_read << ','
           << sample.picking.latest_target_allocations << ',' << sample.window_dimensions.width
           << ',' << sample.window_dimensions.height << ',' << sample.framebuffer_dimensions.width
           << ',' << sample.framebuffer_dimensions.height << ',' << sample.view_dimensions.width
           << ',' << sample.view_dimensions.height << ',' << sample.target_dimensions.width << ','
           << sample.target_dimensions.height << ',' << sample.render_scale_percent << ','
           << csv_flag(sample.vsync_enabled) << ',' << csv_flag(sample.standard_shading) << ",\""
           << state.gl_vendor << "\",\"" << state.gl_renderer << "\",\"" << state.gl_version
           << "\",\"" << state.glsl_version_report << "\"," << state.gl_context_flags << ','
           << state.gl_context_profile_mask << ',' << state.default_red_bits << ','
           << state.default_green_bits << ',' << state.default_blue_bits << ','
           << state.default_alpha_bits << ',' << state.default_depth_bits << ','
           << state.default_stencil_bits << ',' << state.default_samples << ','
           << state.default_srgb_capable << ',' << state.maximum_texture_size << '\n';
}

} // namespace

bool write_performance_csv(ViewerState& state) {
    std::error_code error;
    std::filesystem::create_directories(state.performance_csv_path.parent_path(), error);
    if (error) {
        state.performance_capture_error = error.message();
        return false;
    }
    std::ofstream stream{state.performance_csv_path, std::ios::trunc};
    if (!stream) {
        state.performance_capture_error = "Could not open the performance CSV";
        return false;
    }
    write_csv_header(stream);
    stream << std::fixed << std::setprecision(6);
    for (std::size_t index = 0; index < state.frame_samples.size(); ++index) {
        write_csv_row(stream, index, state.frame_samples[index], state);
    }
    if (!stream) {
        state.performance_capture_error = "Could not finish writing the performance CSV";
        return false;
    }
    state.performance_capture_error.clear();
    return true;
}

} // namespace elf3d::viewer
