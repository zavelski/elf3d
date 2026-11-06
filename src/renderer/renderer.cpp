#include "renderer_cache.hpp"
#include "renderer_detail.h"
#include "renderer_environment_preparation.h"
#include "renderer_pipeline_preparation.h"

#include <renderer/renderer.h>

#include <elf3d/rendering.h>

#include <algorithm>
#include <array>
#include <clipping/clipping.h>
#include <cmath>
#include <cstddef>
#include <graphics/graphics.h>
#include <limits>
#include <math/math.h>
#include <memory>
#include <optional>
#include <scene/scene.h>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace elf3d::renderer {

namespace {
[[nodiscard]] bool has_zero_component(Extent2D extent) noexcept
{
    return extent.width == 0 || extent.height == 0;
}
} // namespace

void apply_clipping_description(const clipping::ClippingFilter& filter,
                                graphics::DrawIndexedDescription& draw) noexcept
{
    draw.clipping_section_plane_enabled = filter.section_plane_enabled;
    draw.clipping_section_plane_normal = filter.section_plane_normal;
    draw.clipping_section_plane_offset = filter.section_plane_offset;
    draw.clipping_retain_positive_half_space = filter.retain_positive_half_space;
    draw.clipping_box_count = filter.enabled_box_count;
    for (std::uint32_t index = 0; index < filter.enabled_box_count; ++index) {
        draw.clipping_boxes[index] = filter.boxes[index];
    }
}

Result<std::unique_ptr<Renderer>>
Renderer::create(std::unique_ptr<graphics::Device> device, std::uint64_t engine_token,
                 std::unique_ptr<StudioEnvironmentSource> environment_source)
{
    if (!device || engine_token == 0 || !environment_source) {
        return Error{ErrorCode::graphics_shutdown,
                     "Renderer creation requires a graphics device, engine identity, and studio "
                     "environment source"};
    }

    Resources resources{std::move(device), engine_token, std::move(environment_source),
                        std::make_unique<CacheState>()};
    return std::make_unique<Renderer>(ConstructionKey{}, std::move(resources));
}

Renderer::Renderer(ConstructionKey, Resources resources) noexcept
    : device_(std::move(resources.device)), engine_token_(resources.engine_token),
      environment_source_(std::move(resources.environment_source)),
      cache_(std::move(resources.cache))
{
}

Renderer::~Renderer() = default;

Result<bool> Renderer::ensure_pipeline_resources(bool extended_materials)
{
    const auto kind = extended_materials ? ShaderPipelineKind::extended : ShaderPipelineKind::core;
    if (!pipelines_)
        pipelines_ = std::make_unique<PipelineResources>();
    auto& selected = pipelines_->ready[static_cast<std::size_t>(kind)];
    if (selected)
        return false;
    if (pipelines_->needs_retirement()) {
        const auto cleaned = complete_retirement();
        if (!cleaned)
            return cleaned.error();
    }
    while (!selected) {
        auto step = pipelines_->advance(*device_, kind, {});
        if (!step) {
            const auto error = step.error();
            const auto cleaned = complete_retirement();
            return cleaned ? Result<bool>{error} : Result<bool>{cleaned.error()};
        }
        if (!step.value().progressed)
            return Error{ErrorCode::invalid_argument,
                         "Synchronous shader preparation did not advance"};
    }
    return true;
}

Result<bool> Renderer::prepare_environment(const RenderRequest& request)
{
    if (request.options.shading_mode != RenderShadingMode::standard) {
        return false;
    }
    return ensure_environment_resources();
}

Result<RenderStatistics> Renderer::render(const scene::Storage& scene_storage,
                                          graphics::RenderTarget& target,
                                          const RenderRequest& request)
{
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene_storage, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return render(scene_storage, target, request, visibility.value());
}

Result<RenderStatistics> Renderer::render(const scene::Storage& scene_storage,
                                          graphics::RenderTarget& target,
                                          const RenderRequest& request,
                                          const scene::VisibilityFilter& visibility)
{
    return render(scene_storage, target, request, visibility, clipping::disabled_filter());
}

Result<RenderStatistics> Renderer::render(const scene::Storage& scene_storage,
                                          graphics::RenderTarget& target,
                                          const RenderRequest& request,
                                          const scene::VisibilityFilter& visibility,
                                          const clipping::ClippingFilter& clipping_filter)
{
    if (!scene_storage.belongs_to_engine(engine_token_)) {
        return Error{ErrorCode::foreign_engine_object,
                     "The scene was created by a different Elf3D engine instance"};
    }
    if (!device_ || !cache_) {
        return Error{ErrorCode::graphics_shutdown, "Renderer graphics resources are unavailable"};
    }

    if (const auto ready = validate_scene_resources(scene_storage); !ready)
        return ready.error();
    const double total_begin = device_->monotonic_time_milliseconds();

    const Result<void> clear_result = target.clear(request.clear_color);
    if (!clear_result) {
        return clear_result.error();
    }
    if (has_zero_component(target.extent())) {
        RenderStatistics statistics;
        statistics.unique_gpu_textures = cache_->texture_count;
        return statistics;
    }

    return execute_render_pass(
        scene_storage, target, request,
        {std::span{&visibility, 1U}, std::span{&clipping_filter, 1U}, total_begin});
}

Result<RenderStatistics> Renderer::execute_render_pass(const scene::Storage& scene_storage,
                                                       graphics::RenderTarget& target,
                                                       const RenderRequest& request,
                                                       const RenderExecutionContext& execution)
{
    const double list_begin = device_->monotonic_time_milliseconds();
    Result<RenderList> list_result =
        build_render_list(scene_storage, request.camera, target.extent(),
                          execution.visibility.front(), execution.clipping_filter.front());
    if (!list_result) {
        return list_result.error();
    }

    const double list_end = device_->monotonic_time_milliseconds();
    RenderPass pass{request, std::move(list_result).value(), execution.clipping_filter.front(), {}};
    bool environment_prepared = false;
    if (!pass.list.items.empty()) {
        Result<bool> environment_result = prepare_environment(request);
        if (!environment_result) {
            return environment_result.error();
        }
        environment_prepared = environment_result.value();
    }
    pass.statistics.cpu_render_list_milliseconds = list_end - list_begin;
    pass.statistics.candidate_primitives = pass.list.candidate_primitives;
    pass.statistics.visible_primitives = static_cast<std::uint64_t>(pass.list.items.size());
    pass.statistics.frustum_culled_primitives = pass.list.frustum_culled_primitives;
    pass.statistics.render_passes = 1;
    pass.statistics.environment_preparations = environment_prepared ? 1U : 0U;
    pass.statistics.clipping_bounds_tested = pass.list.clipping_bounds_tested;
    pass.statistics.clipping_bounds_rejected = pass.list.clipping_bounds_rejected;
    pass.statistics.clipping_bounds_intersecting = pass.list.clipping_bounds_intersecting;
    const double submission_begin = device_->monotonic_time_milliseconds();
    if (!pass.list.items.empty()) {
        const Result<void> items_result = draw_render_items(scene_storage, target, pass);
        if (!items_result) {
            return items_result.error();
        }
    }

    const Result<void> overlay_result = draw_render_overlay(target, pass);
    if (!overlay_result) {
        return overlay_result.error();
    }

    const double submission_end = device_->monotonic_time_milliseconds();
    pass.statistics.cpu_gl_submission_milliseconds = submission_end - submission_begin;
    pass.statistics.estimated_resident_transmission_bytes = target.transmission_scene_bytes();
    pass.statistics.unique_gpu_textures = cache_->texture_count;
    pass.statistics.estimated_resident_geometry_bytes = cache_->resident_geometry_bytes;
    pass.statistics.estimated_resident_texture_bytes = cache_->resident_texture_bytes;
    pass.statistics.estimated_resident_environment_bytes =
        environment_ != nullptr ? environment_->resident_bytes : 0U;
    pass.statistics.shader_switches = static_cast<std::uint64_t>(pass.statistics.draw_calls != 0);
    const graphics::GpuTimingSample main_timing =
        device_->delayed_gpu_timing(graphics::GpuTimingPass::main);
    pass.statistics.gpu_main_pass_milliseconds = main_timing.milliseconds;
    pass.statistics.gpu_main_pass_timing_available = main_timing.available;
    const graphics::GpuTimingSample resolve_timing =
        device_->delayed_gpu_timing(graphics::GpuTimingPass::resolve);
    pass.statistics.gpu_resolve_milliseconds = resolve_timing.milliseconds;
    pass.statistics.gpu_resolve_timing_available = resolve_timing.available;
    pass.statistics.cpu_total_milliseconds = submission_end - execution.total_begin;
    return pass.statistics;
}

[[nodiscard]] bool requires_extended_shader(const graphics::DrawIndexedDescription& draw) noexcept
{
    if (draw.unlit)
        return false;
    return draw.transmission_factor > 0 || draw.clearcoat_factor > 0 ||
           draw.textures[5] != nullptr || draw.textures[6] != nullptr;
}

[[nodiscard]] Result<std::optional<std::size_t>> prepare_transmission_batch(
    graphics::RenderTarget& target, std::span<graphics::DrawIndexedDescription> descriptions,
    RenderStatistics& statistics, std::span<graphics::Texture2D*, 1> snapshot_storage)
{
    std::optional<std::size_t> copy_before;
    const bool required =
        std::any_of(descriptions.begin(), descriptions.end(),
                    [](const auto& draw) { return draw.transmission_factor > 0 && !draw.unlit; });
    if (required) {
        const auto snapshot = target.prepare_transmission_scene();
        if (!snapshot)
            return snapshot.error();
        snapshot_storage.front() = snapshot.value();
        for (std::size_t i = 0; i < descriptions.size(); ++i) {
            if (!descriptions[i].optical_queue)
                continue;
            if (!copy_before)
                copy_before = i;
            descriptions[i].transmission_scene = snapshot_storage;
        }
        statistics.transmission_scene_copies = 1;
        ++statistics.render_passes;
    }
    statistics.estimated_resident_transmission_bytes = target.transmission_scene_bytes();
    return copy_before;
}

Result<void> Renderer::draw_render_items(const scene::Storage& scene,
                                         graphics::RenderTarget& target, RenderPass& pass)
{
    synchronize_draw_packet_cache(scene);
    std::vector<PreparedDraw> prepared;
    prepared.reserve(pass.list.items.size());
    for (const RenderItem& item : pass.list.items) {
        const Result<void> prepare_result = prepare_render_item(scene, item, pass, prepared);
        if (!prepare_result) {
            return prepare_result.error();
        }
    }
    pass.statistics.material_switches = count_material_switches(prepared);

    std::vector<graphics::StaticMesh*> meshes;
    std::vector<graphics::DrawIndexedDescription> descriptions;
    std::vector<std::array<graphics::Texture2D*, graphics::material_texture_count>> texture_sets;
    std::array<graphics::TextureCube*, 2> environment_cubemaps{};
    std::array<graphics::Texture2D*, 1> environment_luts{};
    if (environment_ != nullptr) {
        environment_cubemaps = {environment_->diffuse.get(), environment_->specular.get()};
        environment_luts = {environment_->brdf_lut.get()};
    }

    meshes.reserve(prepared.size());
    descriptions.reserve(prepared.size());
    texture_sets.reserve(prepared.size());
    for (PreparedDraw& draw : prepared) {
        const RenderItem& item = pass.list.items[descriptions.size()];
        const DrawPacket& packet = draw_packet(scene, item, draw.packet_index);
        texture_sets.emplace_back();
        auto& textures = texture_sets.back();
        for (std::size_t index = 0; index < textures.size(); ++index) {
            if (draw.has_textures[index]) {
                textures[index] = &texture(scene.id(), draw.document_images[index],
                                           draw.image_indices[index], draw.texture_indices[index]);
            }
        }
        draw.description.textures = textures;
        draw.description.environment_cubemaps = environment_cubemaps;
        draw.description.environment_luts = environment_luts;
        meshes.push_back(&mesh(scene.id(), packet.document_mesh, packet.mesh_index));
        descriptions.push_back(draw.description);
    }
    const bool extended_materials =
        std::any_of(descriptions.begin(), descriptions.end(), requires_extended_shader);
    const auto pipeline = ensure_pipeline_resources(extended_materials);
    if (!pipeline)
        return pipeline.error();
    // The observer array remains alive through the entire synchronous batch.
    std::array<graphics::Texture2D*, 1> transmission_scene{};
    const auto copy_before =
        prepare_transmission_batch(target, descriptions, pass.statistics, transmission_scene);
    if (!copy_before)
        return copy_before.error();
    auto& selected_pipeline = pipelines_->ready[extended_materials ? 1U : 0U];
    return device_->draw_indexed_batch(target, *selected_pipeline, meshes, descriptions,
                                       copy_before.value());
}

std::uint64_t
Renderer::count_material_switches(const std::vector<PreparedDraw>& prepared) const noexcept
{
    if (prepared.empty()) {
        return 0;
    }

    std::uint64_t switches = 1;
    for (std::size_t index = 1; index < prepared.size(); ++index) {
        switches += static_cast<std::uint64_t>(prepared[index - 1U].material_identity !=
                                               prepared[index].material_identity);
    }
    return switches;
}

Result<void> Renderer::prepare_render_item(const scene::Storage& scene, const RenderItem& item,
                                           RenderPass& pass, std::vector<PreparedDraw>& prepared)
{
    const Result<std::size_t> packet_result =
        cached_draw_packet_index(scene, item, pass.statistics);
    if (!packet_result) {
        return packet_result.error();
    }

    DrawPacket& packet = mutable_draw_packet(scene, item, packet_result.value());
    prepared.emplace_back();
    PreparedDraw& prepared_draw = prepared.back();
    prepared_draw.packet_index = packet_result.value();
    prepared_draw.material_identity = item.material_identity;
    prepared_draw.texture_indices = packet.texture_indices;
    prepared_draw.image_indices = packet.image_indices;
    prepared_draw.document_images = packet.document_images;
    prepared_draw.has_textures = packet.has_textures;
    const Result<void> textures =
        prepare_render_item_textures(scene, item, packet, pass, prepared_draw);
    if (!textures) {
        return textures.error();
    }
    for (const bool has_texture : prepared_draw.has_textures) {
        pass.statistics.texture_bindings += static_cast<std::uint64_t>(has_texture);
    }

    configure_draw_description(packet, item, pass, prepared_draw);

    ++pass.statistics.draw_calls;
    const graphics::StaticMesh& gpu_mesh =
        mesh(scene.id(), packet.document_mesh, packet.mesh_index);
    pass.statistics.vertices += gpu_mesh.vertex_count();
    pass.statistics.indices += gpu_mesh.index_count();
    pass.statistics.triangles += gpu_mesh.index_count() / 3;
    return {};
}

Result<void> Renderer::prepare_render_item_textures(const scene::Storage& scene,
                                                    const RenderItem& item, DrawPacket& packet,
                                                    RenderPass& pass, PreparedDraw& prepared)
{
    const bool standard_lit =
        !packet.material.unlit && pass.request.options.shading_mode == RenderShadingMode::standard;
    if (standard_lit && !packet.lit_textures_prepared) {
        const auto primitive = scene.runtime_primitive(item.entity, item.primitive_index);
        if (!primitive)
            return primitive.error();
        const auto result = prepare_draw_textures(scene, primitive.value(), packet,
                                                  pass.statistics.gpu_texture_uploads, true);
        if (!result)
            return result.error();
        packet.lit_textures_prepared = true;
    }
    prepared.texture_indices = packet.texture_indices;
    prepared.image_indices = packet.image_indices;
    prepared.document_images = packet.document_images;
    prepared.has_textures = packet.has_textures;
    return {};
}

void configure_material_mappings(const MaterialDescription& material,
                                 graphics::DrawIndexedDescription& draw)
{
    draw.texture_mappings = {
        material.base_color_texture_mapping,
        material.metallic_roughness_texture_mapping,
        material.normal_texture_mapping,
        material.occlusion_texture_mapping,
        material.emissive_texture_mapping,
        material.specular_texture_mapping,
        material.specular_color_texture_mapping,
        material.transmission ? material.transmission->texture_mapping : TextureMapping{},
        material.volume ? material.volume->thickness_texture_mapping : TextureMapping{},
        material.clearcoat ? material.clearcoat->texture_mapping : TextureMapping{},
        material.clearcoat ? material.clearcoat->roughness_texture_mapping : TextureMapping{},
        material.clearcoat ? material.clearcoat->normal_texture_mapping : TextureMapping{}};
}

void configure_material_draw(const MaterialDescription& material, bool has_base_normal,
                             bool has_tangent_basis, graphics::DrawIndexedDescription& draw)
{
    draw.metallic_factor = material.metallic_factor;
    draw.roughness_factor = material.roughness_factor;
    draw.normal_scale = material.normal_scale;
    draw.emissive_factor = material.emissive_factor;
    draw.occlusion_strength = material.occlusion_strength;
    draw.ior = material.ior;
    draw.specular_factor = material.specular_factor;
    draw.specular_color_factor = material.specular_color_factor;
    if (material.transmission)
        draw.transmission_factor = material.transmission->factor;
    if (material.volume) {
        draw.thickness_factor = material.volume->thickness_factor;
        draw.attenuation_distance = material.volume->attenuation_distance.value_or(0.0F);
        draw.attenuation_color = material.volume->attenuation_color;
    }
    if (material.clearcoat) {
        draw.clearcoat_factor = material.clearcoat->factor;
        draw.clearcoat_roughness_factor = material.clearcoat->roughness_factor;
        draw.clearcoat_normal_scale = material.clearcoat->normal_scale;
        draw.clearcoat_uses_tangent_basis =
            has_tangent_basis &&
            (!has_base_normal || material.normal_texture_mapping.texcoord_set ==
                                     material.clearcoat->normal_texture_mapping.texcoord_set);
    }
    configure_material_mappings(material, draw);
}

namespace {
void configure_highlight(const RenderRequest& request, const RenderItem& item,
                         graphics::DrawIndexedDescription& draw)
{
    if (request.options.highlight && request.options.highlight->entity == item.entity) {
        draw.highlight_color = request.options.highlight->color;
        draw.highlight_strength = std::clamp(request.options.highlight->strength, 0.0F, 1.0F);
    }
    if (!request.options.primitive_highlight || !item.document_primitive)
        return;
    if (scene::primitive_instance_contains(request.selected_primitives,
                                           {item.entity, *item.document_primitive})) {
        draw.highlight_color = request.options.primitive_highlight->color;
        draw.highlight_strength =
            std::clamp(request.options.primitive_highlight->strength, 0.0F, 1.0F);
    }
}
} // namespace

void Renderer::configure_draw_description(const DrawPacket& packet, const RenderItem& item,
                                          const RenderPass& pass, PreparedDraw& prepared) const
{
    graphics::DrawIndexedDescription& draw = prepared.description;
    draw.model_matrix = item.model_matrix.elements;
    draw.view_matrix = pass.list.view_matrix.elements;
    draw.projection_matrix = pass.list.projection_matrix.elements;
    draw.normal_matrix = item.normal_matrix;
    draw.base_color = packet.material.base_color;
    draw.camera_world_position = pass.list.camera_world_position;
    draw.light_direction = pass.request.lighting.direction;
    draw.light_color = pass.request.lighting.color;
    draw.ambient_intensity = pass.request.lighting.ambient_intensity;
    draw.diffuse_intensity = pass.request.lighting.diffuse_intensity;
    draw.environment_intensity = pass.request.environment_lighting.intensity;
    draw.environment_rotation_radians = pass.request.environment_lighting.rotation_radians;
    configure_material_draw(packet.material, prepared.has_textures[2], packet.has_tangent_basis,
                            draw);
    if (!item.transmission_active)
        draw.transmission_factor = 0;
    configure_highlight(pass.request, item, draw);
    draw.alpha_mode = packet.material.alpha_mode;
    draw.alpha_cutoff = packet.material.alpha_cutoff;
    draw.unlit =
        packet.material.unlit || pass.request.options.shading_mode == RenderShadingMode::unlit;
    draw.optical_queue =
        draw.alpha_mode == AlphaMode::blend || (!draw.unlit && item.transmission_active);
    draw.double_sided = packet.material.double_sided || !pass.request.backface_culling_enabled;
    draw.front_face_clockwise = item.orientation_reversed;
    apply_clipping_description(pass.clipping_filter, draw);
}

void Renderer::synchronize_draw_packet_cache(const scene::Storage& scene)
{
    CacheState::SceneEntry& scene_cache = cache_->scene(scene.id());
    const std::uint64_t revision = scene.render_content_revision();
    if (scene_cache.draw_packet_revision == revision) {
        return;
    }

    scene_cache.draw_packets.clear();
    scene_cache.draw_packet_revision = revision;
}

Result<std::size_t> Renderer::cached_draw_packet_index(const scene::Storage& scene,
                                                       const RenderItem& item,
                                                       RenderStatistics& statistics)
{
    CacheState::SceneEntry& scene_cache = cache_->scene(scene.id());
    const std::size_t entity_index = static_cast<std::size_t>(item.entity.debug_value());
    auto& entity_packets = scene_cache.draw_packets[entity_index];
    const auto found = entity_packets.primitives.find(item.primitive_index);
    if (found != entity_packets.primitives.end())
        return item.primitive_index;

    const Result<scene::RuntimePrimitiveView> primitive =
        scene.runtime_primitive(item.entity, item.primitive_index);
    if (!primitive) {
        return primitive.error();
    }

    Result<std::size_t> mesh_index = cached_mesh(scene.id(), primitive.value(), statistics);
    if (!mesh_index) {
        return mesh_index.error();
    }
    DrawPacket packet;
    packet.mesh_index = mesh_index.value();
    packet.has_tangent_basis = primitive.value().has_tangent_basis();
    packet.document_mesh = primitive.value().document_primitive.is_valid();
    packet.material = runtime_material_description(primitive.value().material_view);
    const Result<void> textures = prepare_draw_textures(scene, primitive.value(), packet,
                                                        statistics.gpu_texture_uploads, false);
    if (!textures) {
        return textures.error();
    }
    entity_packets.primitives.emplace(item.primitive_index, std::move(packet));
    ++statistics.draw_packet_rebuilds;
    return item.primitive_index;
}

Renderer::DrawPacket& Renderer::mutable_draw_packet(const scene::Storage& scene,
                                                    const RenderItem& item,
                                                    std::size_t packet_index) noexcept
{
    auto& entry = cache_->scene(scene.id());
    const auto entity =
        entry.draw_packets.find(static_cast<std::size_t>(item.entity.debug_value()));
    ELF3D_ASSERT(entity != entry.draw_packets.end());
    // packet_index was produced from the runtime uint32 primitive index.
    const auto packet = entity->second.primitives.find(static_cast<std::uint32_t>(packet_index));
    ELF3D_ASSERT(packet != entity->second.primitives.end());
    return packet->second;
}

const Renderer::DrawPacket& Renderer::draw_packet(const scene::Storage& scene,
                                                  const RenderItem& item,
                                                  std::size_t packet_index) const noexcept
{
    const CacheState::SceneEntry& scene_cache = cache_->scene(scene.id());
    const std::size_t entity_index = static_cast<std::size_t>(item.entity.debug_value());
    const auto entity = scene_cache.draw_packets.find(entity_index);
    ELF3D_ASSERT(entity != scene_cache.draw_packets.end());
    const auto packet = entity->second.primitives.find(static_cast<std::uint32_t>(packet_index));
    ELF3D_ASSERT(packet != entity->second.primitives.end());
    return packet->second;
}

Result<void> Renderer::draw_render_overlay(graphics::RenderTarget& target, RenderPass& pass)
{
    const ViewportRenderOptions& options = pass.request.options;
    if (options.overlay_lines.empty() && options.overlay_markers.empty()) {
        return {};
    }

    const graphics::DrawOverlayDescription overlay{pass.list.view_matrix.elements,
                                                   pass.list.projection_matrix.elements,
                                                   options.overlay_lines, options.overlay_markers};
    const Result<void> overlay_result = device_->draw_overlay(target, overlay);
    if (!overlay_result) {
        return overlay_result.error();
    }
    pass.statistics.overlay_lines = static_cast<std::uint64_t>(options.overlay_lines.size());
    pass.statistics.overlay_markers = static_cast<std::uint64_t>(options.overlay_markers.size());
    ++pass.statistics.render_passes;
    return {};
}

graphics::Device& Renderer::device() noexcept
{
    return *device_;
}

const graphics::Device& Renderer::device() const noexcept
{
    return *device_;
}

[[nodiscard]] bool active_material_texture(std::size_t index,
                                           const scene::RuntimeMaterialView& material,
                                           bool standard_lit) noexcept
{
    if (index >= 5 && !standard_lit)
        return false;
    if ((index == 7 || index == 8) && material.transmission_factor <= 0)
        return false;
    if (index >= 9 && material.clearcoat_factor <= 0)
        return false;
    return true;
}

Result<void> Renderer::prepare_draw_textures(const scene::Storage& scene_storage,
                                             const scene::RuntimePrimitiveView& primitive,
                                             DrawPacket& packet, std::uint64_t& upload_count,
                                             bool include_normal_texture)
{
    std::array<scene::RuntimeMaterialTextureSlot, graphics::material_texture_count> texture_slots{};
    for (std::size_t i = 0; i < texture_slots.size(); ++i)
        texture_slots[i] = static_cast<scene::RuntimeMaterialTextureSlot>(i);
    for (std::size_t index = 0; index < texture_slots.size(); ++index) {
        if (!active_material_texture(index, primitive.material_view, include_normal_texture))
            continue;
        if (!primitive.material_view.has_texture(texture_slots[index])) {
            continue;
        }

        const Result<scene::RuntimeTextureView> texture =
            scene_storage.runtime_texture(primitive, texture_slots[index]);
        if (!texture) {
            return texture.error();
        }
        if (index == 2 && (!include_normal_texture || !primitive.has_tangent_basis()))
            continue;

        Result<std::size_t> texture_index = cached_texture(
            scene_storage.id(), texture.value(), texture_color_space(index), upload_count);
        if (!texture_index) {
            return texture_index.error();
        }
        packet.texture_indices[index] = texture_index.value();
        packet.image_indices[index] = static_cast<std::size_t>(texture.value().image_identity);
        packet.document_images[index] = texture.value().document_image;
        packet.has_textures[index] = true;
    }
    return {};
}

graphics::Texture2D& Renderer::texture(SceneId scene_id, bool document_image,
                                       std::size_t image_index,
                                       std::size_t variant_index) const noexcept
{
    const CacheState::SceneEntry& scene_cache = cache_->scene(scene_id);
    const auto& images = document_image ? scene_cache.document_images : scene_cache.images;
    const auto found = images.find(image_index);
    ELF3D_ASSERT(found != images.end() && variant_index < found->second.variants.size());
    return *found->second.variants[variant_index].texture;
}

} // namespace elf3d::renderer
