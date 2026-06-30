#include <elf3d/elf3d.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path &path) {
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t character : utf8) {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

[[nodiscard]] bool is_gltf_file(const std::filesystem::path &path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".gltf" || extension == ".glb";
}

} // namespace

int main(int argument_count, char **arguments) {
    if (argument_count != 2 || arguments[1] == nullptr) {
        std::cerr << "Usage: elf3d_gltf_probe <corpus-directory>\n";
        return 2;
    }

    const std::filesystem::path root{arguments[1]};
    std::error_code error;
    if (!std::filesystem::is_directory(root, error) || error) {
        std::cerr << "Corpus directory is unavailable: " << path_to_utf8(root) << '\n';
        return 2;
    }

    std::vector<std::filesystem::path> files;
    for (std::filesystem::recursive_directory_iterator iterator{
             root, std::filesystem::directory_options::skip_permission_denied, error};
         !error && iterator != std::filesystem::recursive_directory_iterator{};
         iterator.increment(error)) {
        if (iterator->is_regular_file(error) && !error && is_gltf_file(iterator->path())) {
            files.push_back(iterator->path());
        }
    }
    if (error) {
        std::cerr << "Corpus enumeration failed: " << error.message() << '\n';
        return 2;
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        std::cerr << "Corpus contains no .gltf or .glb files: " << path_to_utf8(root) << '\n';
        return 2;
    }

    elf3d::Engine engine;
    std::size_t failure_count = 0;
    std::size_t warning_file_count = 0;
    for (const std::filesystem::path &file : files) {
        const auto started = std::chrono::steady_clock::now();
        elf3d::Result<elf3d::LoadedScene> loaded = engine.load_scene_with_report(file);
        const auto elapsed =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started);

        std::cout << "FILE " << path_to_utf8(file) << '\n';
        std::cout << std::fixed << std::setprecision(3) << "  load_ms=" << elapsed.count() << '\n';
        if (!loaded) {
            ++failure_count;
            std::cout << "  status=FAIL\n";
            std::cout << "  hard_error_code=" << static_cast<int>(loaded.error().code()) << '\n';
            std::cout << "  hard_error=" << loaded.error().message() << '\n';
            continue;
        }

        const elf3d::SceneStatistics statistics = loaded.value().scene->statistics();
        const std::size_t diagnostic_count = loaded.value().report.diagnostics.size();
        if (loaded.value().report.has_warnings()) {
            ++warning_file_count;
        }
        std::cout << "  status=" << (diagnostic_count == 0 ? "PASS" : "PASS_WITH_DIAGNOSTICS")
                  << '\n';
        std::cout << "  entities=" << statistics.entities << " models=" << statistics.model_entities
                  << " primitives=" << statistics.primitives
                  << " triangles=" << statistics.triangles << " vertices=" << statistics.vertices
                  << " materials=" << statistics.material_assets
                  << " textures=" << statistics.texture_assets
                  << " images=" << statistics.image_assets << '\n';
        std::cout << "  textured_materials=" << statistics.materials_with_base_color_textures
                  << " metallic_roughness_textures="
                  << statistics.materials_with_metallic_roughness_textures
                  << " normal_textures=" << statistics.materials_with_normal_textures
                  << " occlusion_textures=" << statistics.materials_with_occlusion_textures
                  << " emissive_textures=" << statistics.materials_with_emissive_textures << '\n';
        for (const elf3d::SceneLoadDiagnostic &diagnostic : loaded.value().report.diagnostics) {
            std::cout << "  diagnostic severity=" << static_cast<int>(diagnostic.severity)
                      << " category=" << static_cast<int>(diagnostic.category)
                      << " code=" << static_cast<int>(diagnostic.code)
                      << " message=" << diagnostic.message;
            if (!diagnostic.source_context.empty()) {
                std::cout << " context=" << diagnostic.source_context;
            }
            std::cout << '\n';
        }
    }

    std::cout << "SUMMARY files=" << files.size() << " failures=" << failure_count
              << " files_with_warnings=" << warning_file_count << '\n';
    return failure_count == 0 ? 0 : 1;
}
