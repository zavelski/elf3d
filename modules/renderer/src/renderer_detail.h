#pragma once

#include <elf3d/internal/renderer.h>

namespace elf3d::renderer {

[[nodiscard]] std::string_view main_vertex_shader_source() noexcept;
[[nodiscard]] std::string_view main_fragment_shader_source() noexcept;

[[nodiscard]] graphics::TextureAddressMode
runtime_address_mode(scene::RuntimeTextureWrap wrap) noexcept;
[[nodiscard]] graphics::TextureFilterMode
runtime_filter_mode(scene::RuntimeTextureFilter filter) noexcept;
[[nodiscard]] MaterialDescription
runtime_material_description(const scene::RuntimeMaterialView& source) noexcept;
struct RuntimeVertexBuffer {
    std::vector<float> values;
    std::uint32_t vertex_count = 0;
    graphics::VertexLayout layout = graphics::VertexLayout::position_normal_float3;
};
[[nodiscard]] RuntimeVertexBuffer
runtime_vertex_buffer(const scene::RuntimePrimitiveView& primitive);

} // namespace elf3d::renderer
