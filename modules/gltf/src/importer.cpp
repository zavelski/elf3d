#include <elf3d/internal/gltf.h>

#include "file_path.hpp"

#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include <elf3d/core/diagnostics.h>
#include <filesystem>
#include <new>
#include <string>
#include <string_view>

namespace elf3d {

Result<LoadedDocument> load_document(std::string_view path_utf8,
                                     const ModelLoadOptions& options) noexcept
{
    Result<std::filesystem::path> path = gltf::file_path::from_utf8(path_utf8);
    if (!path) {
        return path.error();
    }
    return gltf::load_document(path.value(), options);
}

} // namespace elf3d
