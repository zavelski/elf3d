#include "viewer_runtime_state.hpp"

#include <elf3d/app/application.h>

#include <array>
#include <cstdio>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace elf3d;
using namespace elf3d::viewer;

[[nodiscard]] Result<void> failure(const char* message)
{
    return Error{ErrorCode::invalid_argument, message};
}

class FileApplication final : public Application {
  public:
    explicit FileApplication(std::string prefix) : prefix_{std::move(prefix)}
    {
    }
    Result<void> start(ApplicationContext& context) noexcept override
    {
        runtime_.engine = &context.engine();
        runtime_.interaction_arbiter = &context.interaction_arbiter();
        auto viewport = context.engine().create_viewport({});
        auto scene = create_empty_scene(context.engine());
        const auto owner = context.interaction_arbiter().create_owner();
        if (!viewport || !scene || !owner) {
            return failure("Viewer file session initialization failed");
        }
        runtime_.viewport = std::move(viewport).value();
        runtime_.scene = std::move(scene).value();
        runtime_.viewport_interaction_owner = owner.value();
        return {};
    }
    Result<void> update(ApplicationUpdateContext& context) noexcept override
    {
        try {
            runtime_.scene_workflow.begin_frame();
            runtime_.save_workflow.begin_frame();
            runtime_.session_replacement_allowed = true;
            return advance(context);
        } catch (const std::bad_alloc&) {
            fatal_error("Viewer file session allocation failed");
        } catch (...) {
            fatal_error("Unexpected Viewer file session exception");
        }
    }
    Result<void> build_ui(ApplicationUiContext& context) noexcept override
    {
        if (const auto sized = runtime_.viewport->resize({32, 32}); !sized) {
            return sized.error();
        }
        auto queued = context.queue_viewport_render(*runtime_.viewport, *runtime_.scene.scene,
                                                    runtime_.scene.camera);
        runtime_.session_replacement_allowed = false;
        return queued;
    }
    void stop(ApplicationContext& context) noexcept override
    {
        context.interaction_arbiter().destroy_owner(runtime_.viewport_interaction_owner);
        runtime_.viewport.reset();
        runtime_.scene = {};
    }
    [[nodiscard]] bool passed() const noexcept
    {
        return passed_;
    }

  private:
    [[nodiscard]] ViewerWorkflowContext workflow()
    {
        return workflow_context(runtime_);
    }
    [[nodiscard]] Result<void> open()
    {
        runtime_.preferences.storage_path = prefix_ + "-state.ini";
        const auto loaded = execute_scene_workflow(
            workflow(), {SceneReplacementKind::open_model, prefix_ + "-input.glb"});
        if (loaded.error || !loaded.scene_replaced || !runtime_.scene.is_imported()) {
            return failure("Viewer explicit Open failed");
        }
        if (!runtime_.viewport->resize({32, 32})) {
            return failure("Viewer file session resize failed");
        }
        phase_ = 1;
        return runtime_.viewport->fit_to_scene(*runtime_.scene.scene, runtime_.scene.camera);
    }
    [[nodiscard]] Result<PickHit> rendered_selection()
    {
        const auto camera = runtime_.scene.camera;
        const auto hit = runtime_.viewport->pick(*runtime_.scene.scene, camera, {16, 16});
        if (!hit || !hit.value() ||
            !runtime_.viewport->set_selected_entity(*runtime_.scene.scene, hit.value()->entity)) {
            return Error{ErrorCode::invalid_argument,
                         "Viewer file session has no current rendered pick"};
        }
        return *hit.value();
    }
    [[nodiscard]] bool failed_model_retained(const ViewerCommandCompletion& failed, SceneId scene,
                                             EntityId camera,
                                             std::optional<EntityId> selected) const
    {
        return failed.error.has_value() && !failed.scene_replaced &&
               runtime_.scene.scene->id() == scene && runtime_.scene.camera == camera &&
               runtime_.viewport->selected_entity() == selected &&
               runtime_.scene.scene->document().has_value();
    }
    [[nodiscard]] Result<void> failed_open_and_save()
    {
        const auto scene = runtime_.scene.scene->id();
        const auto camera = runtime_.scene.camera;
        const auto hit = rendered_selection();
        if (!hit) {
            return hit.error();
        }
        const auto selected = runtime_.viewport->selected_entity();
        const auto failed = execute_scene_workflow(
            workflow(), {SceneReplacementKind::dropped_file, prefix_ + "-absent.glb"});
        if (!failed_model_retained(failed, scene, camera, selected)) {
            return failure("Viewer failed Open did not preserve current model/handles");
        }
        return hide_and_save(hit.value().entity);
    }
    [[nodiscard]] Result<void> hide_and_save(EntityId entity)
    {
        if (!runtime_.scene.scene->set_entity_local_visibility(entity, false)) {
            return failure("Viewer view-only hide failed");
        }
        const auto saved = execute_save_workflow(workflow(), {prefix_ + "-saved.glb"});
        if (saved.error || runtime_.scene.dirty) {
            return failure("Viewer failed Open disabled Save of the current Document");
        }
        phase_ = 2;
        return {};
    }
    [[nodiscard]] Result<void> reload()
    {
        const auto reloaded = execute_scene_workflow(
            workflow(), {SceneReplacementKind::reload_model, prefix_ + "-saved.glb"});
        if (reloaded.error || !reloaded.scene_replaced || runtime_.viewport->selected_entity() ||
            runtime_.scene.source_statistics.triangles != 1 ||
            runtime_.scene.expanded_triangles != 1) {
            return failure("Viewer Save/reload lost hidden geometry or retained old selection");
        }
        phase_ = 3;
        return {};
    }
    [[nodiscard]] Result<void> advance(ApplicationUpdateContext& context)
    {
        switch (phase_) {
        case 0:
            return open();
        case 1:
            return failed_open_and_save();
        case 2:
            return reload();
        case 3: {
            const auto closed =
                execute_scene_workflow(workflow(), {SceneReplacementKind::close_to_empty, {}});
            if (closed.error || runtime_.scene.is_imported()) {
                return failure("Viewer Close did not leave empty state");
            }
            ++phase_;
            return {};
        }
        default:
            const auto stats = runtime_.viewport->render_statistics();
            if (stats.estimated_resident_geometry_bytes || stats.estimated_resident_texture_bytes) {
                return failure("Viewer Close retained GPU model resources");
            }
            passed_ = true;
            context.request_exit();
            return {};
        }
    }
    std::string prefix_;
    ViewerAssembly runtime_;
    unsigned phase_ = 0;
    bool passed_ = false;
};

[[nodiscard]] bool write_fixture(const std::string& path)
{
    Document document;
    const auto scene = document.create_scene("Viewer file fixture");
    const auto mesh = document.create_mesh("triangle");
    const auto node = document.create_node("triangle");
    const auto material = document.create_material({});
    if (!scene || !mesh || !node || !material) {
        return false;
    }
    PrimitiveData data;
    data.positions = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}};
    data.indices = {0, 1, 2};
    return document.create_primitive(mesh.value(), material.value(), std::move(data)) &&
           document.set_node_mesh(node.value(), mesh.value()) &&
           document.add_scene_root(scene.value(), node.value()) &&
           save_document(path, document.view());
}
} // namespace

int run_viewer_session_tests(const char* output_prefix)
{
    const std::string prefix{output_prefix};
    if (!write_fixture(prefix + "-input.glb")) {
        return 1;
    }
    FileApplication application{prefix};
    ApplicationOptions options;
    options.title = "Viewer file workflow session";
    options.initial_window_extent = {64, 64};
    options.initial_visibility = ApplicationWindowVisibility::hidden;
    const auto result = run_application(options, application);
    if (!result) {
        std::fprintf(stderr, "Viewer session: %s\n", result.error().message());
        const auto code = result.error().code();
        return code == ErrorCode::graphics_initialization_failed ||
                       code == ErrorCode::graphics_context_unavailable ||
                       code == ErrorCode::unsupported_graphics_version
                   ? 77
                   : 1;
    }
    return application.passed() ? 0 : 1;
}
