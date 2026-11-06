#include "prepared_scene_checks.h"
#include "primitive_visibility_checks.h"
#include "runtime_hierarchy_checks.h"
#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>
#include <elf3d/model.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace {
int fail(int code, const char* message)
{
    std::cerr << message << '\n';
    return code;
}
[[nodiscard]] int verify_model_import(const elf3d::LoadedScene& loaded)
{
    const elf3d::SceneStatistics expected_statistics{1, 1, 2, 2, 2, 8, 12, 4, 1, 2, 2, 16, 2, 1};
    if (loaded.report.diagnostic_count() != 0 ||
        loaded.scene->statistics() != expected_statistics) {
        return fail(4, "Embedded smoke model produced unexpected import facts");
    }
    return 0;
}

bool valid_render_statistics(const elf3d::RenderStatistics& statistics) noexcept
{
    return !(statistics.draw_calls != 2 || statistics.triangles != 4 || statistics.vertices != 8 ||
             statistics.indices != 12 || statistics.texture_bindings != 3 ||
             statistics.gpu_texture_uploads != 3 || statistics.unique_gpu_textures != 3);
}

elf3d::Error failure(const char* message) noexcept
{
    return {elf3d::ErrorCode::invalid_argument, message};
}

elf3d::Result<void> verify_invalid_scene_selection(elf3d::Engine& engine, elf3d::Document& document,
                                                   elf3d::DocumentSceneId selected) noexcept
{
    if (engine.create_scene_from_document(std::move(document), {})) {
        return failure("Invalid selection unexpectedly accepted");
    }
    elf3d::Document foreign;
    const auto foreign_id = foreign.create_scene();
    if (!foreign_id) {
        return foreign_id.error();
    }
    if (engine.create_scene_from_document(std::move(document), foreign_id.value()) ||
        !document.scene(selected)) {
        return failure("Foreign selection changed source");
    }
    return {};
}

elf3d::Result<void> verify_empty_scene_retirement(elf3d::Scene& scene,
                                                  elf3d::DocumentSceneId selected) noexcept
{
    auto returned = scene.take_document();
    if (!returned || scene.id().is_valid() || scene.document() || scene.take_document() ||
        !returned.value().scene(selected)) {
        return failure("Empty scene extraction failed");
    }
    return {};
}

elf3d::Result<void> verify_empty_scene_transfer(elf3d::Engine& engine) noexcept
{
    elf3d::Document empty;
    const auto selected = empty.create_scene("Empty selected scene");
    if (!selected || !empty.clear_default_scene()) {
        return failure("Empty fixture preparation failed");
    }
    if (const auto rejected = verify_invalid_scene_selection(engine, empty, selected.value());
        !rejected) {
        return rejected.error();
    }
    auto scene = engine.create_scene_from_document(std::move(empty), selected.value());
    if (!scene || !scene.value()->document() || scene.value()->document()->default_scene() ||
        empty.scene_count() != 0) {
        return failure("Empty scene/default state changed");
    }
    return verify_empty_scene_retirement(*scene.value(), selected.value());
}

elf3d::Result<void> verify_procedural_cpu_retirement(elf3d::Scene& scene) noexcept
{
    auto retired = scene.retire_cpu_state();
    if (!retired || retired.value().empty() || retired.value().has_document() ||
        retired.value().take_document() || retired.value().empty() || scene.id().is_valid()) {
        return failure("Procedural CPU retirement invented a Document or lost its owner");
    }
    return {};
}

elf3d::Result<void> verify_procedural_document_absence(elf3d::Engine& engine) noexcept
{
    auto procedural = engine.create_scene();
    if (!procedural) {
        return procedural.error();
    }
    const auto procedural_id = procedural.value()->id();
    if (procedural.value()->document() || procedural.value()->take_document() ||
        procedural.value()->id() != procedural_id || !procedural.value()->create_entity()) {
        return failure("Procedural scene changed by failed extraction");
    }
    return verify_procedural_cpu_retirement(*procedural.value());
}

elf3d::Result<elf3d::Document> make_unsupported_runtime_document() noexcept
{
    // An empty mesh is valid canonical data, but the runtime requires at least
    // one primitive for a model entity. This fails after the adapter adopts it.
    elf3d::Document unsupported;
    const auto selected = unsupported.create_scene();
    const auto node = unsupported.create_node("Retained after late failure");
    const auto mesh = unsupported.create_mesh();
    if (!selected || !node || !mesh || !unsupported.set_node_mesh(node.value(), mesh.value()) ||
        !unsupported.add_scene_root(selected.value(), node.value()) ||
        elf3d::validate_document(unsupported.view()).has_errors()) {
        return failure("Invalid late-failure fixture");
    }
    return unsupported;
}

elf3d::Result<void> verify_late_adoption_failure(elf3d::Engine& engine) noexcept
{
    auto made = make_unsupported_runtime_document();
    if (!made) {
        return made.error();
    }
    auto document = std::move(made).value();
    const auto selected = document.scene_at(0).value().id;
    const auto node = document.node_at(0).value().id;
    const auto failed = engine.create_scene_from_document(std::move(document), selected);
    if (failed || !document.scene(selected) || !document.node(node) ||
        document.node(node).value().name != "Retained after late failure") {
        return failure("Late failure lost the Document");
    }
    return {};
}

bool has_original_payload(elf3d::DocumentView view, elf3d::PrimitiveView primitive,
                          elf3d::ImageView image,
                          std::optional<elf3d::DocumentSceneId> authored_default) noexcept
{
    const auto owned_primitive = view.primitive(primitive.id);
    const auto owned_image = view.image(image.id);
    return owned_primitive && owned_image &&
           owned_primitive.value().data.positions.data() == primitive.data.positions.data() &&
           owned_image.value().pixels.data() == image.pixels.data() &&
           owned_image.value().source_bytes.data() == image.source_bytes.data() &&
           view.default_scene() == authored_default;
}

elf3d::Result<void> verify_document_scene_errors(elf3d::Engine& engine) noexcept
{
    if (const auto result = verify_empty_scene_transfer(engine); !result) {
        return result.error();
    }
    if (const auto result = verify_procedural_document_absence(engine); !result) {
        return result.error();
    }
    return verify_late_adoption_failure(engine);
}

elf3d::Result<void> verify_public_document_mapping(elf3d::Scene& scene) noexcept
{
    const auto view = *scene.document();
    const auto node = view.node_at(0).value().id;
    const auto mapped = scene.entity_for_document_node(node);
    if (!mapped || !mapped.value() || scene.document_node(*mapped.value()).value() != node ||
        scene.document_primitive(*mapped.value(), 0).value() != view.primitive_at(0).value().id) {
        return failure("Public Document/runtime provenance did not resolve");
    }
    const auto runtime_only = scene.create_entity();
    if (!runtime_only || scene.document_node(runtime_only.value()).value()) {
        return failure("Runtime-only object acquired a fabricated Document node");
    }
    return {};
}

elf3d::Result<elf3d::SceneCpuRetirement> retire_original_scene(elf3d::Scene& scene) noexcept
{
    auto retired = scene.retire_cpu_state();
    if (!retired || retired.value().empty() || !retired.value().has_document() ||
        scene.id().is_valid() || scene.document() || scene.retire_cpu_state()) {
        return failure("CPU retirement did not transfer ownership and invalidate the Scene");
    }
    return retired;
}

elf3d::Result<elf3d::Document> extract_original_document(std::unique_ptr<elf3d::Scene>& scene,
                                                         elf3d::PrimitiveView primitive,
                                                         elf3d::ImageView image) noexcept
{
    auto retired = retire_original_scene(*scene);
    if (!retired) {
        return retired.error();
    }
    auto owner = std::move(retired).value();
    if (!retired.value().empty()) {
        return failure("Moved CPU retirement retained a second owner");
    }
    auto detached = owner.take_document();
    if (!owner.empty() || owner.take_document()) {
        return failure("CPU Document recovery retained a populated retirement owner");
    }
    scene.reset();
    if (!detached || !detached.value().primitive(primitive.id) ||
        !has_original_payload(detached.value().view(), primitive, image,
                              detached.value().default_scene())) {
        return failure("Extraction changed payload or identity");
    }
    return detached;
}

elf3d::Result<elf3d::Document> verify_document_scene_ownership(elf3d::Engine& engine) noexcept
{
    auto loaded =
        elf3d::load_document(ELF3D_TEST_SOURCE_DIR "/tests/fixtures/elf3d_smoke/elf3d_smoke.gltf");
    if (!loaded) {
        return loaded.error();
    }
    auto& document = loaded.value().document;
    const auto primitive = document.primitive_at(0).value();
    const auto image = document.image_at(0).value();
    const auto authored_default = document.default_scene();
    auto created =
        engine.create_scene_from_document(std::move(document), loaded.value().default_scene);
    if (!created || !created.value()->document() ||
        !elf3d::validate_document(document.view()).has_errors() ||
        engine.create_scene_from_document(std::move(document), loaded.value().default_scene)) {
        return failure("Adoption or moved-from input contract failed");
    }
    const auto view = *created.value()->document();
    if (!has_original_payload(view, primitive, image, authored_default)) {
        return failure("Adoption copied payload or changed IDs/default");
    }
    if (const auto mapped = verify_public_document_mapping(*created.value()); !mapped) {
        return mapped.error();
    }
    return extract_original_document(created.value(), primitive, image);
}

elf3d::Result<elf3d::PreparedSceneDocument> prepare_contract_survivor(elf3d::Engine& engine)
{
    if (const auto checked = elf3d::test::verify_primitive_visibility_contract(engine); !checked)
        return checked.error();
    if (const auto checked = elf3d::test::verify_runtime_hierarchy_contract(engine); !checked) {
        return checked.error();
    }
    if (const auto checked = elf3d::test::verify_prepared_scene_contract(engine); !checked) {
        return checked.error();
    }
    return elf3d::test::prepare_scene_survivor(engine);
}

class ModelApplication final : public elf3d::Application {
  public:
    elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override
    {
        auto survivor = prepare_contract_survivor(context.engine());
        if (!survivor) {
            return survivor.error();
        }
        prepared_survivor_ = std::move(survivor).value();
        // Pull the source-integrated Model implementation into the same consumer
        // as the runtime DLL, including the shared core diagnostic functions.
        const auto node = document_.create_node("Model SDK alive alongside the runtime scene");
        if (!node) {
            return node.error();
        }
        if (const auto errors = verify_document_scene_errors(context.engine()); !errors) {
            return errors.error();
        }
        auto detached = verify_document_scene_ownership(context.engine());
        if (!detached) {
            return detached.error();
        }
        document_ = std::move(detached).value();
        constexpr std::string_view path =
            ELF3D_TEST_SOURCE_DIR "/tests/fixtures/elf3d_smoke/elf3d_smoke.gltf";
        auto loaded = context.engine().load_scene(path);
        if (!loaded) {
            return loaded.error();
        }
        if (verify_model_import(loaded.value()) != 0) {
            return elf3d::Error{elf3d::ErrorCode::scene_import_failed, "Unexpected model facts"};
        }
        scene_ = std::move(loaded).value().scene;
        auto camera = scene_->create_perspective_camera_entity({});
        auto viewport = context.engine().create_viewport({64, 64});
        if (!camera) {
            return camera.error();
        }
        if (!viewport) {
            return viewport.error();
        }
        camera_ = camera.value();
        viewport_ = std::move(viewport).value();
        viewport_->set_clear_color({0, 0, 0, 1});
        return viewport_->reset_view(*scene_, camera_);
    }
    elf3d::Result<void> update(elf3d::ApplicationUpdateContext& context) noexcept override
    {
        if (!context.previous_frame_statistics()) {
            return {};
        }
        if (!valid_render_statistics(viewport_->render_statistics())) {
            return elf3d::Error{elf3d::ErrorCode::draw_submission_failed,
                                "Unexpected model render statistics"};
        }
        std::array<std::uint8_t, 64U * 64U * 4U> pixels{};
        const auto readback = viewport_->read_color_pixels(pixels);
        if (!readback) {
            return readback.error();
        }
        std::size_t colored = 0;
        for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4U) {
            if (pixels[pixel] > 8U || pixels[pixel + 1U] > 8U || pixels[pixel + 2U] > 8U) {
                ++colored;
            }
        }
        if (colored < 256U) {
            return elf3d::Error{elf3d::ErrorCode::draw_submission_failed,
                                "Model did not produce enough visible pixels"};
        }
        if (elf3d::validate_document(document_.view()).has_errors()) {
            return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                "Model SDK document is invalid"};
        }
        passed_ = true;
        context.request_exit();
        return {};
    }
    elf3d::Result<void> build_ui(elf3d::ApplicationUiContext& context) noexcept override
    {
        return context.queue_viewport_render(*viewport_, *scene_, camera_);
    }
    void stop(elf3d::ApplicationContext&) noexcept override
    {
        viewport_.reset();
        scene_.reset();
        // The detached canonical owner deliberately survives framework teardown.
    }
    bool passed() const noexcept
    {
        return passed_ && document_.image_count() != 0 &&
               !elf3d::validate_document(document_.view()).has_errors();
    }
    elf3d::PreparedSceneDocument take_preparation() noexcept
    {
        return std::move(prepared_survivor_);
    }

  private:
    elf3d::Document document_;
    elf3d::PreparedSceneDocument prepared_survivor_;
    std::unique_ptr<elf3d::Scene> scene_;
    std::unique_ptr<elf3d::Viewport> viewport_;
    elf3d::EntityId camera_;
    bool passed_ = false;
};
} // namespace

int main()
{
    ModelApplication application;
    elf3d::ApplicationOptions options;
    options.initial_window_extent = {64, 64};
    options.initial_visibility = elf3d::ApplicationWindowVisibility::hidden;
    options.presentation_mode = elf3d::PresentationMode::immediate;
    const auto result = elf3d::run_application(options, application);
    if (!result) {
        const auto code = result.error().code();
        if (code == elf3d::ErrorCode::graphics_initialization_failed ||
            code == elf3d::ErrorCode::graphics_context_unavailable ||
            code == elf3d::ErrorCode::unsupported_graphics_version) {
            std::cout << "SKIP: " << result.error().message() << '\n';
            return 77;
        }
        std::cerr << result.error().message() << '\n';
        return 1;
    }
    if (!application.passed()) {
        return 1;
    }
    return elf3d::test::verify_foreign_scene_preparation(application.take_preparation());
}
