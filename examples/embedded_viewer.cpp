#include <elf3d/elf3d.h>

namespace elf3d_examples {

[[nodiscard]] elf3d::Result<elf3d::NativeTextureView>
render_embedded_frame(elf3d::Engine& engine, elf3d::Scene& scene, elf3d::Viewport& viewport,
                      elf3d::EntityId camera_entity, const elf3d::ViewportInput& input) noexcept {
    const elf3d::Result<void> navigation = viewport.update_navigation(scene, camera_entity, input);
    if (!navigation) {
        return navigation.error();
    }

    const elf3d::Result<void> rendered = viewport.render(scene, camera_entity);
    if (!rendered) {
        return rendered.error();
    }

    return engine.native_texture_view(viewport.color_texture());
}

} // namespace elf3d_examples
