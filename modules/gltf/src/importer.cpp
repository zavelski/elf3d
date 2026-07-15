module;

#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include <filesystem>
#include <new>
#include <string>
#include <string_view>

module elf.gltf;

import elf.core;
import elf.model;

namespace elf3d {

Result<LoadedDocument> load_document(std::string_view path_utf8,
                                     const ModelLoadOptions& options) noexcept {
    try {
        std::u8string utf8;
        utf8.reserve(path_utf8.size());
        for (const char character : path_utf8) {
            utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
        }
        return gltf::load_document(std::filesystem::path{utf8}, options);
    } catch (const std::bad_alloc&) {
        fatal_error("Elf3D glTF importer memory allocation failed");
    } catch (...) {
        fatal_error("Elf3D glTF importer encountered an unexpected exception");
    }
}

} // namespace elf3d
