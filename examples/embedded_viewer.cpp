#include <elf3d/embed/runtime.h>

namespace elf3d_examples {

[[nodiscard]] elf3d::Result<elf3d::NativeTextureView>
render_embedded_frame(elf3d::EmbeddedRuntime& runtime, elf3d::Scene& scene,
                      elf3d::Viewport& viewport, elf3d::EntityId camera_entity,
                      const elf3d::NavigationInput& input) noexcept {
    const elf3d::Result<void> navigation = viewport.update_navigation(scene, camera_entity, input);
    if (!navigation) {
        return navigation.error();
    }

    const elf3d::Result<void> rendered = viewport.render(scene, camera_entity);
    if (!rendered) {
        return rendered.error();
    }

    return runtime.native_texture_view(viewport.color_texture());
}

} // namespace elf3d_examples
