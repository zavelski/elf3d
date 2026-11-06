#pragma once

#include <renderer/renderer.h>

namespace elf3d::renderer {
[[nodiscard]] bool active_material_texture(std::size_t index,
                                           const scene::RuntimeMaterialView& material,
                                           bool standard_lit) noexcept;

[[nodiscard]] std::string_view main_vertex_shader_source() noexcept;
[[nodiscard]] std::string_view main_fragment_shader_source() noexcept;

[[nodiscard]] graphics::TextureAddressMode
runtime_address_mode(scene::RuntimeTextureWrap wrap) noexcept;
[[nodiscard]] graphics::TextureFilterMode
runtime_filter_mode(scene::RuntimeTextureFilter filter) noexcept;
[[nodiscard]] AlphaMode runtime_alpha_mode(scene::RuntimeAlphaMode mode) noexcept;
[[nodiscard]] MaterialDescription
runtime_material_description(const scene::RuntimeMaterialView& source) noexcept;

} // namespace elf3d::renderer
