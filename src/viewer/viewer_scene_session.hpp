#pragma once

#include <elf3d/elf3d.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace elf3d::viewer {

struct SceneSession {
    std::unique_ptr<Scene> scene;
    EntityId camera;
    std::filesystem::path source_path;
    SceneStatistics source_statistics;
    std::optional<Bounds3> source_bounds;
    bool camera_needs_reset = true;
    bool dirty = false;
    std::uint64_t saved_document_revision = 0;
    std::optional<std::filesystem::path> save_target;
    ModelWriteOptions save_options;
    SceneLoadReport load_report;
    std::uint64_t expanded_triangles = 0;

    [[nodiscard]] bool has_unsaved_changes() const noexcept
    {
        const auto document = scene ? scene->document() : std::nullopt;
        return document && (dirty || (saved_document_revision != 0 &&
                                      document->revision() != saved_document_revision));
    }

    [[nodiscard]] bool is_imported() const noexcept
    {
        return !source_path.empty();
    }
};

[[nodiscard]] std::uint64_t document_instance_triangles(DocumentView document) noexcept;

} // namespace elf3d::viewer
