#include <elf3d/model.h>

#include <string_view>
#include <utility>

namespace elf3d_examples {

[[nodiscard]] elf3d::Result<elf3d::ModelWriteReport>
validate_and_save_document(std::string_view input_path_utf8, std::string_view output_path_utf8,
                           const elf3d::ModelLoadOptions& load_options = {},
                           const elf3d::ModelWriteOptions& write_options = {}) noexcept {
    elf3d::Result<elf3d::LoadedDocument> loaded_result =
        elf3d::load_document(input_path_utf8, load_options);
    if (!loaded_result) {
        return loaded_result.error();
    }

    elf3d::LoadedDocument loaded = std::move(loaded_result).value();
    const elf3d::DocumentValidationReport validation =
        elf3d::validate_document(loaded.document.view());
    if (validation.has_errors()) {
        return elf3d::Error{elf3d::ErrorCode::gltf_validation_failed,
                            "The loaded document failed validation"};
    }

    return elf3d::save_document(output_path_utf8, loaded.document.view(), write_options);
}

} // namespace elf3d_examples
