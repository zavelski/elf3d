#include "prepared_scene_document.h"
#include "scene_load_report.h"

#include <core/exception_boundary.h>

#include <memory>
#include <utility>

namespace elf3d {
PreparedSceneDocument::PreparedSceneDocument() noexcept = default;
PreparedSceneDocument::~PreparedSceneDocument() noexcept = default;
PreparedSceneDocument::PreparedSceneDocument(std::unique_ptr<Impl> impl) noexcept
    : impl_{std::move(impl)}
{
}
PreparedSceneDocument::PreparedSceneDocument(PreparedSceneDocument&&) noexcept = default;
PreparedSceneDocument& PreparedSceneDocument::operator=(PreparedSceneDocument&&) noexcept = default;

Result<PreparedSceneDocument> PreparedSceneDocument::reserve(std::uint64_t engine_token,
                                                             std::uint64_t scene_value) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<PreparedSceneDocument> {
        if (engine_token == 0 || scene_value == 0) {
            return Error{ErrorCode::invalid_argument,
                         "CPU preparation requires a reserved Scene identity"};
        }
        return PreparedSceneDocument{std::make_unique<Impl>(
            detail::SceneHandleAccess::create_scene(engine_token, scene_value))};
    });
}

bool PreparedSceneDocument::empty() const noexcept
{
    return impl_ == nullptr;
}
bool PreparedSceneDocument::ready() const noexcept
{
    return impl_ && impl_->storage;
}
bool PreparedSceneDocument::belongs_to_engine(std::uint64_t engine_token) const noexcept
{
    return impl_ && detail::SceneHandleAccess::engine_token(impl_->id) == engine_token;
}

Result<void> PreparedSceneDocument::prepare(Document&& document, DocumentSceneId selected_scene,
                                            const ScenePreparationOptions& options) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        if (empty() || ready()) {
            return Error{ErrorCode::invalid_argument,
                         "CPU preparation requires an unused Scene reservation"};
        }
        if (!document.scene(selected_scene)) {
            return Error{ErrorCode::invalid_argument,
                         "In-memory scene selection must belong to the input document"};
        }
        if (validate_document(document.view()).has_errors()) {
            return Error{ErrorCode::invalid_argument,
                         "In-memory scene creation requires a valid canonical document"};
        }
        auto storage = std::make_unique<scene::Storage>(impl_->id);
        auto populated =
            scene::populate_from_document(std::move(document), selected_scene, *storage);
        if (populated) {
            populated = storage->reserve_runtime_entities(options.additional_runtime_entities);
        }
        if (!populated) {
            if (storage->has_document()) {
                document = storage->release_document();
            }
            return populated.error();
        }
        // Warm geometry-scale bounds and world-transform caches before transfer.
        // No Scene/context/renderer service is involved in these CPU operations.
        impl_->bounds = storage->world_bounds();
        impl_->statistics = storage->statistics();
        impl_->storage = std::move(storage);
        return {};
    });
}

std::optional<DocumentView> PreparedSceneDocument::document() const noexcept
{
    return ready() ? std::optional<DocumentView>{impl_->storage->document()} : std::nullopt;
}

Result<SceneLoadReport>
PreparedSceneDocument::load(std::string_view path_utf8, const ModelLoadOptions& options,
                            const ScenePreparationOptions& preparation) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<SceneLoadReport> {
        if (empty() || ready()) {
            return Error{ErrorCode::invalid_argument,
                         "File loading requires an unused Scene reservation"};
        }
        auto loaded = load_document(path_utf8, options);
        if (!loaded) {
            return loaded.error();
        }
        const auto prepared =
            prepare(std::move(loaded.value().document), loaded.value().default_scene, preparation);
        if (!prepared) {
            return prepared.error();
        }
        auto report = std::make_unique<SceneLoadReport::Impl>();
        report->diagnostics = std::move(loaded.value().report.diagnostics);
        return SceneLoadReport{std::move(report)};
    });
}
SceneStatistics PreparedSceneDocument::statistics() const noexcept
{
    return ready() ? impl_->statistics : SceneStatistics{};
}
std::optional<Bounds3> PreparedSceneDocument::world_bounds() const noexcept
{
    return ready() ? impl_->bounds : std::nullopt;
}
Result<Document> PreparedSceneDocument::take_document() noexcept
{
    if (!ready()) {
        return Error{ErrorCode::invalid_argument,
                     "Document recovery requires completed CPU preparation"};
    }
    auto document = impl_->storage->release_document();
    impl_.reset();
    return document;
}
} // namespace elf3d
