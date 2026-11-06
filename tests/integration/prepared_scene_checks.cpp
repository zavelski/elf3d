#include "prepared_scene_checks.h"

#include <elf3d/app/application.h>

#include <cstdio>
#include <limits>
#include <utility>

namespace elf3d::test {
namespace {
constexpr std::string_view source =
    ELF3D_TEST_SOURCE_DIR "/tests/fixtures/elf3d_smoke/elf3d_smoke.gltf";
[[nodiscard]] Error failure(const char* message) noexcept
{
    return {ErrorCode::invalid_argument, message};
}

[[nodiscard]] Result<void> verify_preparation_failure(PreparedSceneDocument& prepared,
                                                      LoadedDocument& loaded,
                                                      PrimitiveView original) noexcept
{
    auto& document = loaded.document;
    if (prepared.prepare(std::move(document), {}) || !document.primitive(original.id)) {
        return failure("Rejected preparation changed the canonical source");
    }
    const auto overflow =
        prepared.prepare(std::move(document), loaded.default_scene,
                         {.additional_runtime_entities = std::numeric_limits<std::size_t>::max()});
    if (overflow || prepared.ready() ||
        document.primitive(original.id).value().data.positions.data() !=
            original.data.positions.data()) {
        return failure("Late capacity failure did not recover the exact canonical owner");
    }
    return {};
}

[[nodiscard]] Result<void> verify_installed_owner(Engine& engine, PreparedSceneDocument& prepared,
                                                  PrimitiveView original) noexcept
{
    const auto statistics = prepared.statistics();
    const auto bounds = prepared.world_bounds();
    auto installed = engine.install_prepared_document(std::move(prepared));
    if (!installed || !prepared.empty() || installed.value()->statistics() != statistics ||
        installed.value()->world_bounds() != bounds) {
        return failure("Installation did not consume prepared state with matching cached values");
    }
    const auto primitive = installed.value()->document()->primitive(original.id);
    if (!primitive || primitive.value().data.positions.data() != original.data.positions.data() ||
        primitive.value().data.indices.data() != original.data.indices.data()) {
        return failure("CPU preparation/installation cloned canonical geometry");
    }
    return {};
}

[[nodiscard]] Result<void> verify_recovery(Engine& engine) noexcept
{
    auto prepared = prepare_scene_survivor(engine);
    if (!prepared) {
        return prepared.error();
    }
    const auto original = prepared.value().document()->image_at(0).value();
    auto document = prepared.value().take_document();
    if (!document || !prepared.value().empty() || prepared.value().take_document()) {
        return failure("Prepared Document recovery did not consume exactly one owner");
    }
    const auto image = document.value().image(original.id);
    if (!image || image.value().pixels.data() != original.pixels.data() ||
        image.value().source_bytes.data() != original.source_bytes.data()) {
        return failure("Prepared recovery changed retained image storage");
    }
    return {};
}

class ForeignPreparationApplication final : public Application {
  public:
    explicit ForeignPreparationApplication(PreparedSceneDocument&& prepared) noexcept
        : prepared_{std::move(prepared)}
    {
    }
    Result<void> start(ApplicationContext& context) noexcept override
    {
        const auto view = prepared_.document();
        if (!view) {
            return failure("Prepared CPU owner did not survive Engine teardown");
        }
        const auto original = view->primitive_at(0).value();
        if (context.engine().install_prepared_document(std::move(prepared_)) ||
            !prepared_.ready()) {
            return failure("A foreign Engine consumed a CPU preparation reservation");
        }
        auto document = prepared_.take_document();
        if (!document || document.value().primitive(original.id).value().data.positions.data() !=
                             original.data.positions.data()) {
            return failure("Foreign rejection lost the original canonical owner");
        }
        passed_ = true;
        return {};
    }
    Result<void> update(ApplicationUpdateContext& context) noexcept override
    {
        context.request_exit();
        return {};
    }
    Result<void> build_ui(ApplicationUiContext&) noexcept override
    {
        return {};
    }
    void stop(ApplicationContext&) noexcept override
    {
    }
    bool passed() const noexcept
    {
        return passed_;
    }

  private:
    PreparedSceneDocument prepared_;
    bool passed_ = false;
};
} // namespace

Result<PreparedSceneDocument> prepare_scene_survivor(Engine& engine) noexcept
{
    auto prepared = engine.create_scene_preparation();
    if (!prepared) {
        return prepared.error();
    }
    auto report = prepared.value().load(source);
    if (!report) {
        return report.error();
    }
    if (!prepared.value().ready() || report.value().diagnostic_count() != 0) {
        return failure("CPU file preparation changed supported import diagnostics");
    }
    return prepared;
}

Result<void> verify_prepared_scene_contract(Engine& engine) noexcept
{
    auto loaded = load_document(source);
    auto prepared = engine.create_scene_preparation();
    if (!loaded || !prepared) {
        return failure("Cannot prepare the public ownership fixture");
    }
    const auto original = loaded.value().document.primitive_at(0).value();
    if (prepared.value().ready() || prepared.value().document() ||
        engine.install_prepared_document(std::move(prepared).value())) {
        return failure("An unused reservation was treated as a populated CPU owner");
    }
    auto& document = loaded.value().document;
    if (const auto checked = verify_preparation_failure(prepared.value(), loaded.value(), original);
        !checked) {
        return checked.error();
    }
    const auto prepared_ok =
        prepared.value().prepare(std::move(document), loaded.value().default_scene);
    if (!prepared_ok) {
        return prepared_ok.error();
    }
    if (const auto verified = verify_installed_owner(engine, prepared.value(), original);
        !verified) {
        return verified.error();
    }
    return verify_recovery(engine);
}

int verify_foreign_scene_preparation(PreparedSceneDocument&& prepared)
{
    ForeignPreparationApplication application{std::move(prepared)};
    ApplicationOptions options;
    options.initial_visibility = ApplicationWindowVisibility::hidden;
    options.presentation_mode = PresentationMode::immediate;
    const auto result = run_application(options, application);
    if (!result) {
        std::fprintf(stderr, "Foreign CPU preparation: %s\n", result.error().message());
        return 1;
    }
    return application.passed() ? 0 : 1;
}
} // namespace elf3d::test
