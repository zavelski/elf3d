module;

#include <elf3d/graphics.h>

#include <glad/gl.h>

#include "device_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

module elf.backend.opengl;

import elf.graphics;

namespace elf3d::backend::opengl::device_detail {
namespace {

class OpenGLTexture2D final : public graphics::Texture2D {
  public:
    OpenGLTexture2D(std::shared_ptr<OpenGLDeviceState> state, GLuint texture,
                    Extent2D extent) noexcept
        : state_(std::move(state)), texture_(texture), extent_(extent) {}

    ~OpenGLTexture2D() override {
        if (state_->can_destroy_objects() && texture_ != 0) {
            glDeleteTextures(1, &texture_);
        }
    }

    [[nodiscard]] Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return opengl_resource_token();
    }

    [[nodiscard]] GLuint texture() const noexcept {
        return texture_;
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    GLuint texture_ = 0;
    Extent2D extent_;
};

class OpenGLTextureCube final : public graphics::TextureCube {
  public:
    OpenGLTextureCube(std::shared_ptr<OpenGLDeviceState> state, GLuint texture,
                      std::uint32_t extent, std::uint32_t mip_count) noexcept
        : state_(std::move(state)), texture_(texture), extent_(extent), mip_count_(mip_count) {}

    ~OpenGLTextureCube() override {
        if (state_->can_destroy_objects() && texture_ != 0) {
            glDeleteTextures(1, &texture_);
        }
    }

    [[nodiscard]] std::uint32_t extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] std::uint32_t mip_count() const noexcept override {
        return mip_count_;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return opengl_resource_token();
    }

    [[nodiscard]] GLuint texture() const noexcept {
        return texture_;
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    GLuint texture_ = 0;
    std::uint32_t extent_ = 0;
    std::uint32_t mip_count_ = 0;
};

GLenum texture_wrap(graphics::TextureAddressMode mode) noexcept {
    switch (mode) {
    case graphics::TextureAddressMode::repeat:
        return GL_REPEAT;
    case graphics::TextureAddressMode::mirrored_repeat:
        return GL_MIRRORED_REPEAT;
    case graphics::TextureAddressMode::clamp_to_edge:
        return GL_CLAMP_TO_EDGE;
    }
    return GL_REPEAT;
}

GLenum texture_filter(graphics::TextureFilterMode mode) noexcept {
    switch (mode) {
    case graphics::TextureFilterMode::nearest:
        return GL_NEAREST;
    case graphics::TextureFilterMode::linear:
        return GL_LINEAR;
    case graphics::TextureFilterMode::nearest_mipmap_nearest:
        return GL_NEAREST_MIPMAP_NEAREST;
    case graphics::TextureFilterMode::linear_mipmap_nearest:
        return GL_LINEAR_MIPMAP_NEAREST;
    case graphics::TextureFilterMode::nearest_mipmap_linear:
        return GL_NEAREST_MIPMAP_LINEAR;
    case graphics::TextureFilterMode::linear_mipmap_linear:
        return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_LINEAR;
}

bool uses_mipmaps(graphics::TextureFilterMode mode) noexcept {
    return mode != graphics::TextureFilterMode::nearest &&
           mode != graphics::TextureFilterMode::linear;
}

struct OpenGLMeshObjects {
    GLuint vertex_array = 0;
    GLuint vertex_buffer = 0;
    GLuint index_buffer = 0;
};

class OpenGLStaticMesh final : public graphics::StaticMesh {
  public:
    OpenGLStaticMesh(std::shared_ptr<OpenGLDeviceState> state, OpenGLMeshObjects objects,
                     std::uint32_t vertex_count, std::uint32_t index_count,
                     graphics::VertexLayout vertex_layout) noexcept {
        state_ = std::move(state);
        vertex_array_ = objects.vertex_array;
        vertex_buffer_ = objects.vertex_buffer;
        index_buffer_ = objects.index_buffer;
        vertex_count_ = vertex_count;
        index_count_ = index_count;
        vertex_layout_ = vertex_layout;
    }

    ~OpenGLStaticMesh() override {
        if (!state_->can_destroy_objects()) {
            return;
        }
        glDeleteVertexArrays(1, &vertex_array_);
        glDeleteBuffers(1, &vertex_buffer_);
        glDeleteBuffers(1, &index_buffer_);
    }

    [[nodiscard]] std::uint32_t vertex_count() const noexcept override {
        return vertex_count_;
    }

    [[nodiscard]] std::uint32_t index_count() const noexcept override {
        return index_count_;
    }

    [[nodiscard]] graphics::VertexLayout vertex_layout() const noexcept override {
        return vertex_layout_;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return opengl_resource_token();
    }

    [[nodiscard]] GLuint vertex_array() const noexcept {
        return vertex_array_;
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;
    GLuint index_buffer_ = 0;
    std::uint32_t vertex_count_ = 0;
    std::uint32_t index_count_ = 0;
    graphics::VertexLayout vertex_layout_ = graphics::VertexLayout::position_normal_float3;
};

class OpenGLGraphicsPipeline final : public graphics::GraphicsPipeline {
  public:
    OpenGLGraphicsPipeline(std::shared_ptr<OpenGLDeviceState> state, GLuint program,
                           UniformLocations uniforms) noexcept
        : state_(std::move(state)), program_(program), uniforms_(uniforms) {}

    ~OpenGLGraphicsPipeline() override {
        if (state_->can_destroy_objects() && program_ != 0) {
            glDeleteProgram(program_);
        }
    }

    [[nodiscard]] GLuint program() const noexcept {
        return program_;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return opengl_resource_token();
    }

    [[nodiscard]] const UniformLocations& uniforms() const noexcept {
        return uniforms_;
    }

    [[nodiscard]] std::optional<float>& environment_intensity() noexcept {
        return environment_intensity_;
    }

    [[nodiscard]] std::optional<float>& environment_rotation() noexcept {
        return environment_rotation_;
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    GLuint program_ = 0;
    UniformLocations uniforms_;
    std::optional<float> environment_intensity_;
    std::optional<float> environment_rotation_;
};

[[nodiscard]] Result<std::size_t> mesh_vertex_stride(graphics::VertexLayout layout) noexcept {
    constexpr graphics::VertexLayout tangent_layout =
        graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4_tangent_float4;
    switch (layout) {
    case graphics::VertexLayout::position_normal_float3:
        return sizeof(float) * 6;
    case graphics::VertexLayout::position_normal_float3_texcoord_float2:
        return sizeof(float) * 8;
    case graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4:
        return sizeof(float) * 14;
    case tangent_layout:
        return sizeof(float) * 18;
    }
    return Error{ErrorCode::unsupported_vertex_layout,
                 "The OpenGL backend does not support the requested vertex layout"};
}

[[nodiscard]] bool valid_mesh_sizes(const graphics::StaticMeshDescription& description,
                                    std::size_t vertex_stride) noexcept {
    return description.vertex_count != 0 && !description.indices.empty() &&
           description.vertex_bytes.size() ==
               static_cast<std::size_t>(description.vertex_count) * vertex_stride &&
           description.indices.size() <=
               static_cast<std::size_t>(std::numeric_limits<GLsizei>::max());
}

void delete_mesh_objects(const OpenGLMeshObjects& objects) noexcept {
    glDeleteVertexArrays(1, &objects.vertex_array);
    glDeleteBuffers(1, &objects.vertex_buffer);
    glDeleteBuffers(1, &objects.index_buffer);
}

[[nodiscard]] Result<OpenGLMeshObjects> allocate_mesh_objects() noexcept {
    OpenGLMeshObjects objects;
    glGenVertexArrays(1, &objects.vertex_array);
    glGenBuffers(1, &objects.vertex_buffer);
    glGenBuffers(1, &objects.index_buffer);
    if (objects.vertex_array == 0 || objects.vertex_buffer == 0 || objects.index_buffer == 0) {
        delete_mesh_objects(objects);
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "OpenGL failed to allocate static mesh objects"};
    }
    return objects;
}

void upload_mesh_data(const OpenGLMeshObjects& objects,
                      const graphics::StaticMeshDescription& description) noexcept {
    glBindVertexArray(objects.vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, objects.vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(description.vertex_bytes.size()),
                 description.vertex_bytes.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, objects.index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(description.indices.size_bytes()),
                 description.indices.data(), GL_STATIC_DRAW);
}

void configure_mesh_attributes(graphics::VertexLayout layout, std::size_t vertex_stride) noexcept {
    const GLsizei stride = static_cast<GLsizei>(vertex_stride);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(sizeof(float) * 3));
    if (layout != graphics::VertexLayout::position_normal_float3) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(sizeof(float) * 6));
    }
    if (layout == graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4 ||
        layout == graphics::VertexLayout::
                      position_normal_float3_texcoord2_float2_color_float4_tangent_float4) {
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(sizeof(float) * 8));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(sizeof(float) * 10));
    }
    if (layout == graphics::VertexLayout::
                      position_normal_float3_texcoord2_float2_color_float4_tangent_float4) {
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(sizeof(float) * 14));
    }
}

[[nodiscard]] Result<GLint> texture_internal_format(graphics::TextureFormat format) noexcept {
    switch (format) {
    case graphics::TextureFormat::rgba8_srgb:
        return GL_SRGB8_ALPHA8;
    case graphics::TextureFormat::rgba8_unorm:
        return GL_RGBA8;
    case graphics::TextureFormat::rgba16_float:
        return GL_RGBA16F;
    default:
        return Error{ErrorCode::unsupported_texture_format,
                     "OpenGL texture upload does not support the requested format"};
    }
}

[[nodiscard]] std::size_t texture_bytes_per_pixel(graphics::TextureFormat format) noexcept {
    return format == graphics::TextureFormat::rgba16_float ? 8U : 4U;
}

[[nodiscard]] Result<void>
validate_texture_description(const graphics::Texture2DDescription& description,
                             const OpenGLDeviceState& state) noexcept {
    if (description.extent.width == 0 || description.extent.height == 0 ||
        !state.supports(description.extent)) {
        return Error{ErrorCode::gpu_texture_creation_failed,
                     "Texture dimensions are zero or exceed OpenGL limits"};
    }
    const std::size_t width = description.extent.width;
    const std::size_t height = description.extent.height;
    const std::size_t bytes_per_pixel = texture_bytes_per_pixel(description.format);
    if (width > std::numeric_limits<std::size_t>::max() / bytes_per_pixel ||
        height > std::numeric_limits<std::size_t>::max() / (width * bytes_per_pixel) ||
        description.pixels.size() != width * height * bytes_per_pixel) {
        return Error{ErrorCode::gpu_texture_upload_failed,
                     "Texture upload does not contain tightly packed pixels"};
    }
    if (description.mag_filter != graphics::TextureFilterMode::nearest &&
        description.mag_filter != graphics::TextureFilterMode::linear) {
        return Error{ErrorCode::invalid_sampler_filter,
                     "Texture magnification supports only nearest or linear filtering"};
    }
    return {};
}

void upload_texture(GLuint texture, const graphics::Texture2DDescription& description,
                    GLint internal_format) noexcept {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    static_cast<GLint>(texture_wrap(description.wrap_u)));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    static_cast<GLint>(texture_wrap(description.wrap_v)));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    static_cast<GLint>(texture_filter(description.min_filter)));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    static_cast<GLint>(texture_filter(description.mag_filter)));
    const GLenum type = description.format == graphics::TextureFormat::rgba16_float
                            ? GL_HALF_FLOAT
                            : GL_UNSIGNED_BYTE;
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, static_cast<GLsizei>(description.extent.width),
                 static_cast<GLsizei>(description.extent.height), 0, GL_RGBA, type,
                 description.pixels.data());
    if (uses_mipmaps(description.min_filter)) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
}

[[nodiscard]] Result<GLuint>
create_texture_object(const graphics::Texture2DDescription& description,
                      GLint internal_format) noexcept {
    AllocationStateGuard state_guard;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        return Error{ErrorCode::gpu_texture_creation_failed,
                     "OpenGL failed to allocate a 2D texture object"};
    }
    upload_texture(texture, description, internal_format);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return Error{ErrorCode::gpu_texture_upload_failed,
                     "OpenGL reported an error while uploading a 2D texture"};
    }
    return texture;
}

[[nodiscard]] Result<void> validate_texture_cube_mip(const graphics::TextureCubeMipDescription& mip,
                                                     std::uint32_t expected_extent,
                                                     const OpenGLDeviceState& state) noexcept {
    if (mip.extent == 0 || mip.extent != expected_extent ||
        !state.supports({mip.extent, mip.extent})) {
        return Error{ErrorCode::gpu_texture_creation_failed,
                     "Cubemap mip dimensions are invalid or exceed OpenGL limits"};
    }
    const std::size_t extent = mip.extent;
    if (extent > std::numeric_limits<std::size_t>::max() / 8U ||
        extent > std::numeric_limits<std::size_t>::max() / (extent * 8U)) {
        return Error{ErrorCode::size_overflow, "Cubemap mip byte size overflows"};
    }
    const std::size_t expected_bytes = extent * extent * 8U;
    for (const std::span<const std::byte> face : mip.faces) {
        if (face.size() != expected_bytes) {
            return Error{ErrorCode::gpu_texture_upload_failed,
                         "Every cubemap mip must provide six tightly packed RGBA16F faces"};
        }
    }
    return {};
}

[[nodiscard]] Result<void>
validate_texture_cube_description(const graphics::TextureCubeDescription& description,
                                  const OpenGLDeviceState& state) noexcept {
    if (description.format != graphics::TextureFormat::rgba16_float || description.mips.empty()) {
        return Error{ErrorCode::unsupported_texture_format,
                     "Studio cubemaps require one or more RGBA16F mip levels"};
    }
    if (description.mag_filter != graphics::TextureFilterMode::nearest &&
        description.mag_filter != graphics::TextureFilterMode::linear) {
        return Error{ErrorCode::invalid_sampler_filter,
                     "Cubemap magnification supports only nearest or linear filtering"};
    }
    std::uint32_t expected_extent = description.mips.front().extent;
    for (const graphics::TextureCubeMipDescription& mip : description.mips) {
        const Result<void> mip_validation = validate_texture_cube_mip(mip, expected_extent, state);
        if (!mip_validation) {
            return mip_validation.error();
        }
        expected_extent = std::max(expected_extent / 2U, 1U);
    }
    return {};
}

[[nodiscard]] Result<GLuint>
create_texture_cube_object(const graphics::TextureCubeDescription& description) noexcept {
    AllocationStateGuard state_guard;
    while (glGetError() != GL_NO_ERROR) {
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        return Error{ErrorCode::gpu_texture_creation_failed,
                     "OpenGL failed to allocate a cubemap texture object"};
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                    static_cast<GLint>(texture_filter(description.min_filter)));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,
                    static_cast<GLint>(texture_filter(description.mag_filter)));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL,
                    static_cast<GLint>(description.mips.size() - 1U));
    for (std::size_t level = 0; level < description.mips.size(); ++level) {
        const graphics::TextureCubeMipDescription& mip = description.mips[level];
        for (std::size_t face = 0; face < mip.faces.size(); ++face) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face),
                         static_cast<GLint>(level), GL_RGBA16F, static_cast<GLsizei>(mip.extent),
                         static_cast<GLsizei>(mip.extent), 0, GL_RGBA, GL_HALF_FLOAT,
                         mip.faces[face].data());
        }
    }
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return Error{ErrorCode::gpu_texture_upload_failed,
                     "OpenGL reported an error while uploading a cubemap"};
    }
    return texture;
}

[[nodiscard]] Result<GLuint>
create_graphics_program(const graphics::GraphicsPipelineDescription& description) {
    return create_program_from_sources(description.vertex_shader_source,
                                       description.fragment_shader_source);
}

[[nodiscard]] UniformLocations query_uniform_locations(GLuint program) noexcept {
    return UniformLocations{glGetUniformLocation(program, "u_model"),
                            glGetUniformLocation(program, "u_view"),
                            glGetUniformLocation(program, "u_projection"),
                            glGetUniformLocation(program, "u_normal_matrix"),
                            glGetUniformLocation(program, "u_vertex_layout"),
                            glGetUniformLocation(program, "u_orientation_sign"),
                            glGetUniformLocation(program, "u_base_color"),
                            glGetUniformLocation(program, "u_camera_world_position"),
                            glGetUniformLocation(program, "u_light_direction"),
                            glGetUniformLocation(program, "u_light_color"),
                            glGetUniformLocation(program, "u_ambient_intensity"),
                            glGetUniformLocation(program, "u_diffuse_intensity"),
                            glGetUniformLocation(program, "u_environment_intensity"),
                            glGetUniformLocation(program, "u_environment_rotation"),
                            glGetUniformLocation(program, "u_metallic_factor"),
                            glGetUniformLocation(program, "u_roughness_factor"),
                            glGetUniformLocation(program, "u_emissive_factor"),
                            glGetUniformLocation(program, "u_occlusion_strength"),
                            glGetUniformLocation(program, "u_normal_scale"),
                            glGetUniformLocation(program, "u_ior"),
                            glGetUniformLocation(program, "u_specular_factor"),
                            glGetUniformLocation(program, "u_specular_color_factor"),
                            glGetUniformLocation(program, "u_highlight_color"),
                            glGetUniformLocation(program, "u_highlight_strength"),
                            glGetUniformLocation(program, "u_has_base_color_texture"),
                            glGetUniformLocation(program, "u_has_metallic_roughness_texture"),
                            glGetUniformLocation(program, "u_has_normal_texture"),
                            glGetUniformLocation(program, "u_has_occlusion_texture"),
                            glGetUniformLocation(program, "u_has_emissive_texture"),
                            glGetUniformLocation(program, "u_base_color_texture"),
                            glGetUniformLocation(program, "u_metallic_roughness_texture"),
                            glGetUniformLocation(program, "u_normal_texture"),
                            glGetUniformLocation(program, "u_occlusion_texture"),
                            glGetUniformLocation(program, "u_emissive_texture"),
                            glGetUniformLocation(program, "u_diffuse_environment"),
                            glGetUniformLocation(program, "u_specular_environment"),
                            glGetUniformLocation(program, "u_environment_brdf_lut"),
                            glGetUniformLocation(program, "u_texture_texcoord_sets[0]"),
                            glGetUniformLocation(program, "u_texture_offsets[0]"),
                            glGetUniformLocation(program, "u_texture_scales[0]"),
                            glGetUniformLocation(program, "u_texture_rotations[0]"),
                            glGetUniformLocation(program, "u_alpha_mode"),
                            glGetUniformLocation(program, "u_alpha_cutoff"),
                            glGetUniformLocation(program, "u_unlit"),
                            glGetUniformLocation(program, "u_clipping_section_plane_enabled"),
                            glGetUniformLocation(program, "u_clipping_section_plane_normal"),
                            glGetUniformLocation(program, "u_clipping_section_plane_offset"),
                            glGetUniformLocation(program, "u_clipping_retain_positive_half_space"),
                            glGetUniformLocation(program, "u_clipping_box_count"),
                            glGetUniformLocation(program, "u_clipping_box_minimums[0]"),
                            glGetUniformLocation(program, "u_clipping_box_maximums[0]")};
}

[[nodiscard]] bool transform_locations_valid(const UniformLocations& locations) noexcept {
    return locations.model >= 0 && locations.view >= 0 && locations.projection >= 0 &&
           locations.normal >= 0 && locations.vertex_layout >= 0 &&
           locations.orientation_sign >= 0 && locations.base_color >= 0;
}

[[nodiscard]] bool direct_lighting_locations_valid(const UniformLocations& locations) noexcept {
    return locations.camera_world_position >= 0 && locations.light_direction >= 0 &&
           locations.light_color >= 0 && locations.ambient_intensity >= 0 &&
           locations.diffuse_intensity >= 0;
}

[[nodiscard]] bool indirect_lighting_locations_valid(const UniformLocations& locations) noexcept {
    return locations.environment_intensity >= 0 && locations.environment_rotation >= 0 &&
           locations.metallic_factor >= 0 && locations.roughness_factor >= 0 &&
           locations.emissive_factor >= 0 && locations.occlusion_strength >= 0 &&
           locations.normal_scale >= 0;
}

[[nodiscard]] bool lighting_locations_valid(const UniformLocations& locations) noexcept {
    return direct_lighting_locations_valid(locations) &&
           indirect_lighting_locations_valid(locations);
}

[[nodiscard]] bool surface_locations_valid(const UniformLocations& locations) noexcept {
    return locations.ior >= 0 && locations.specular_factor >= 0 &&
           locations.specular_color_factor >= 0 && locations.highlight_color >= 0 &&
           locations.highlight_strength >= 0;
}

[[nodiscard]] bool material_texture_locations_valid(const UniformLocations& locations) noexcept {
    return locations.has_base_color_texture >= 0 && locations.has_metallic_roughness_texture >= 0 &&
           locations.has_normal_texture >= 0 && locations.has_occlusion_texture >= 0 &&
           locations.has_emissive_texture >= 0 && locations.base_color_texture >= 0 &&
           locations.metallic_roughness_texture >= 0 && locations.normal_texture >= 0 &&
           locations.occlusion_texture >= 0 && locations.emissive_texture >= 0;
}

[[nodiscard]] bool texture_unit_locations_valid(const UniformLocations& locations) noexcept {
    return material_texture_locations_valid(locations) && locations.diffuse_environment >= 0 &&
           locations.specular_environment >= 0 && locations.environment_brdf_lut >= 0;
}

[[nodiscard]] bool texture_parameter_locations_valid(const UniformLocations& locations) noexcept {
    return locations.texture_texcoord_sets >= 0 && locations.texture_offsets >= 0 &&
           locations.texture_scales >= 0 && locations.texture_rotations >= 0 &&
           locations.alpha_mode >= 0 && locations.alpha_cutoff >= 0 && locations.unlit >= 0;
}

[[nodiscard]] bool clipping_locations_valid(const UniformLocations& locations) noexcept {
    return locations.clipping_section_plane_enabled >= 0 &&
           locations.clipping_section_plane_normal >= 0 &&
           locations.clipping_section_plane_offset >= 0 &&
           locations.clipping_retain_positive_half_space >= 0 &&
           locations.clipping_box_count >= 0 && locations.clipping_box_minimums >= 0 &&
           locations.clipping_box_maximums >= 0;
}

void configure_texture_sampler_uniforms(GLuint program, const UniformLocations& uniforms) noexcept {
    const std::array<GLint, 8> locations{
        uniforms.base_color_texture,   uniforms.metallic_roughness_texture,
        uniforms.normal_texture,       uniforms.occlusion_texture,
        uniforms.emissive_texture,     uniforms.diffuse_environment,
        uniforms.specular_environment, uniforms.environment_brdf_lut,
    };
    for (std::size_t index = 0; index < locations.size(); ++index) {
        glProgramUniform1i(program, locations[index], static_cast<GLint>(index));
    }
}

} // namespace

bool UniformLocations::valid() const noexcept {
    return transform_locations_valid(*this) && lighting_locations_valid(*this) &&
           surface_locations_valid(*this) && texture_unit_locations_valid(*this) &&
           texture_parameter_locations_valid(*this) && clipping_locations_valid(*this);
}

Result<std::unique_ptr<graphics::StaticMesh>>
create_static_mesh(std::shared_ptr<OpenGLDeviceState> state,
                   const graphics::StaticMeshDescription& description) noexcept {
    const Result<std::size_t> stride_result = mesh_vertex_stride(description.vertex_layout);
    if (!stride_result) {
        return stride_result.error();
    }
    const std::size_t vertex_stride = stride_result.value();
    if (!valid_mesh_sizes(description, vertex_stride)) {
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "Static mesh upload contains invalid vertex or index sizes"};
    }

    try {
        AllocationStateGuard state_guard;
        Result<OpenGLMeshObjects> objects_result = allocate_mesh_objects();
        if (!objects_result) {
            return objects_result.error();
        }
        const OpenGLMeshObjects objects = objects_result.value();
        upload_mesh_data(objects, description);
        configure_mesh_attributes(description.vertex_layout, vertex_stride);
        if (glGetError() != GL_NO_ERROR) {
            delete_mesh_objects(objects);
            return Error{ErrorCode::gpu_buffer_creation_failed,
                         "OpenGL reported an error while uploading static mesh buffers"};
        }

        return std::unique_ptr<graphics::StaticMesh>{std::make_unique<OpenGLStaticMesh>(
            std::move(state), objects, description.vertex_count,
            static_cast<std::uint32_t>(description.indices.size()), description.vertex_layout)};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

Result<std::unique_ptr<graphics::Texture2D>>
create_texture_2d(std::shared_ptr<OpenGLDeviceState> state,
                  const graphics::Texture2DDescription& description) noexcept {
    const Result<void> description_validation = validate_texture_description(description, *state);
    if (!description_validation) {
        return description_validation.error();
    }
    const Result<GLint> format_result = texture_internal_format(description.format);
    if (!format_result) {
        return format_result.error();
    }

    try {
        Result<GLuint> texture_result = create_texture_object(description, format_result.value());
        if (!texture_result) {
            return texture_result.error();
        }
        return std::unique_ptr<graphics::Texture2D>{std::make_unique<OpenGLTexture2D>(
            std::move(state), texture_result.value(), description.extent)};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

Result<std::unique_ptr<graphics::TextureCube>>
create_texture_cube(std::shared_ptr<OpenGLDeviceState> state,
                    const graphics::TextureCubeDescription& description) noexcept {
    const Result<void> description_validation =
        validate_texture_cube_description(description, *state);
    if (!description_validation) {
        return description_validation.error();
    }
    try {
        Result<GLuint> texture_result = create_texture_cube_object(description);
        if (!texture_result) {
            return texture_result.error();
        }
        return std::unique_ptr<graphics::TextureCube>{std::make_unique<OpenGLTextureCube>(
            std::move(state), texture_result.value(), description.mips.front().extent,
            static_cast<std::uint32_t>(description.mips.size()))};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

Result<std::unique_ptr<graphics::GraphicsPipeline>>
create_graphics_pipeline(std::shared_ptr<OpenGLDeviceState> state,
                         const graphics::GraphicsPipelineDescription& description) noexcept {
    if (description.vertex_layout !=
        graphics::VertexLayout::
            position_normal_float3_texcoord2_float2_color_float4_tangent_float4) {
        return Error{ErrorCode::unsupported_vertex_layout,
                     "The PBR pipeline requires position, normal, two UV sets, color, and tangent"};
    }

    try {
        Result<GLuint> program_result = create_graphics_program(description);
        if (!program_result) {
            return program_result.error();
        }
        const GLuint program = program_result.value();
        const UniformLocations uniforms = query_uniform_locations(program);
        if (!uniforms.valid()) {
            glDeleteProgram(program);
            return Error{ErrorCode::shader_linking_failed,
                         "The linked shader program is missing a required renderer uniform"};
        }
        configure_texture_sampler_uniforms(program, uniforms);
        return std::unique_ptr<graphics::GraphicsPipeline>{
            std::make_unique<OpenGLGraphicsPipeline>(std::move(state), program, uniforms)};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

Result<MeshView> mesh_view(graphics::StaticMesh& mesh) noexcept {
    if (mesh.backend_resource_token() != opengl_resource_token()) {
        return Error{ErrorCode::backend_mismatch, "The static mesh does not belong to OpenGL"};
    }
    auto& opengl_mesh = static_cast<OpenGLStaticMesh&>(mesh);
    return MeshView{opengl_mesh.vertex_array(), opengl_mesh.index_count(),
                    static_cast<std::uint8_t>(opengl_mesh.vertex_layout())};
}

Result<PipelineView> pipeline_view(graphics::GraphicsPipeline& pipeline) noexcept {
    if (pipeline.backend_resource_token() != opengl_resource_token()) {
        return Error{ErrorCode::backend_mismatch,
                     "The graphics pipeline does not belong to OpenGL"};
    }
    auto& opengl_pipeline = static_cast<OpenGLGraphicsPipeline&>(pipeline);
    return PipelineView{opengl_pipeline.program(), opengl_pipeline.uniforms(),
                        &opengl_pipeline.environment_intensity(),
                        &opengl_pipeline.environment_rotation()};
}

Result<GLuint> texture_object(const graphics::Texture2D* texture) noexcept {
    if (texture == nullptr) {
        return GLuint{0};
    }
    if (!is_opengl_texture(texture)) {
        return Error{ErrorCode::backend_mismatch, "The material texture does not belong to OpenGL"};
    }
    return static_cast<const OpenGLTexture2D*>(texture)->texture();
}

Result<GLuint> texture_cube_object(const graphics::TextureCube* texture) noexcept {
    if (texture == nullptr) {
        return GLuint{0};
    }
    if (texture->backend_resource_token() != opengl_resource_token()) {
        return Error{ErrorCode::backend_mismatch,
                     "The environment cubemap does not belong to OpenGL"};
    }
    return static_cast<const OpenGLTextureCube*>(texture)->texture();
}

} // namespace elf3d::backend::opengl::device_detail
