import elf.assets;
import elf.backend.opengl;
import elf.clipping;
import elf.core;
import elf.gltf;
import elf.graphics;
import elf.image;
import elf.interaction;
import elf.math;
import elf.navigation;
import elf.picking;
import elf.renderer;
import elf.scene;
import elf.tool.clipping;
import elf.tool.measurement;
import elf.tool.selection;
import elf.tool.visibility;
import elf.viewport;

int main() {
    const auto version = elf3d::core::version_data();
    if (version.major != 0U || version.minor != 7U || version.patch != 1U) {
        return 1;
    }

    [[maybe_unused]] elf3d::assets::MeshAsset mesh;
    [[maybe_unused]] elf3d::clipping::ClippingFilter clipping_filter;
    [[maybe_unused]] elf3d::gltf::ImportReport import_report;
    [[maybe_unused]] elf3d::graphics::DrawIndexedDescription draw_description;
    [[maybe_unused]] elf3d::image::DecodedImage decoded_image;
    [[maybe_unused]] elf3d::interaction::ViewportInteractionFrame interaction_frame;
    [[maybe_unused]] elf3d::navigation::NavigationUpdate navigation_update;
    [[maybe_unused]] elf3d::picking::RayBoundsHit ray_bounds_hit;
    [[maybe_unused]] elf3d::renderer::GpuPickResult gpu_pick_result;
    [[maybe_unused]] elf3d::scene::VisibilityFilter visibility_filter;
    [[maybe_unused]] elf3d::tools::clipping::ClippingOverlay clipping_overlay;
    [[maybe_unused]] elf3d::tools::measurement::DisplayLength display_length;

    [[maybe_unused]] auto *create_opengl_device = &elf3d::backend::opengl::create_device;
    [[maybe_unused]] auto *clamp_color = &elf3d::math::clamp_color;
    [[maybe_unused]] auto *selection_controller =
        static_cast<elf3d::tools::selection::SelectionController *>(nullptr);
    [[maybe_unused]] auto *visibility_controller =
        static_cast<elf3d::tools::visibility::VisibilityController *>(nullptr);
    [[maybe_unused]] auto *offscreen_viewport =
        static_cast<elf3d::viewport::OffscreenViewport *>(nullptr);

    return 0;
}
