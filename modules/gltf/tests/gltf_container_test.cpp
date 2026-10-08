#include <elf3d/model.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <elf3d/internal/gltf.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gltf_import_test_support.h"

namespace elf3d::gltf::import_tests {

[[nodiscard]] int test_data_uri_buffer(const TemporaryDirectory& temporary)
{
    const std::vector<std::byte> positions = triangle_positions();
    const std::filesystem::path path = temporary.path() / "data_buffer.gltf";
    const std::string member = "\"uri\":\"data:application/octet-stream;base64," +
                               base64(positions) + "\",\"byteLength\":36";
    if (!write_text(path, simple_triangle_json(member))) {
        return 1;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (!loaded ||
        !has_diagnostic(loaded.value().report, elf3d::ModelLoadDiagnosticCode::generated_normals) ||
        loaded.value().document.default_scene().has_value()) {
        return 2;
    }
    return 0;
}

[[nodiscard]] int test_glb_container(const TemporaryDirectory& temporary)
{
    const std::filesystem::path path = temporary.path() / "triangle.glb";
    const std::vector<std::byte> positions = triangle_positions();
    const std::string json = simple_triangle_json("\"byteLength\":36");
    if (!write_bytes(path, make_glb(json, positions)) || !elf3d::load_document(path.string())) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int test_implicit_scene(const TemporaryDirectory& temporary)
{
    const std::filesystem::path path = temporary.path() / "implicit_scene.gltf";
    const std::string json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"plain.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}]})json";
    if (!write_bytes(temporary.path() / "plain.bin", triangle_positions()) ||
        !write_text(path, json)) {
        return 4;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (!loaded) {
        return 5;
    }

    const auto scene = loaded.value().document.scene_at(0U);
    if (!scene || loaded.value().document.scene_count() != 1U ||
        loaded.value().document.default_scene().has_value() ||
        loaded.value().default_scene != scene.value().id || scene.value().roots.size() != 1U) {
        return 5;
    }
    return 0;
}

[[nodiscard]] int test_embedded_image(const TemporaryDirectory& temporary)
{
    std::vector<std::byte> embedded = textured_geometry();
    embedded.insert(embedded.end(), std::as_bytes(std::span{asymmetric_png}).begin(),
                    std::as_bytes(std::span{asymmetric_png}).end());
    const std::string json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"byteLength":173}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":77}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],"images":[{"bufferView":3,"mimeType":"image/png"}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    const std::filesystem::path path = temporary.path() / "image.glb";
    if (!write_bytes(path, make_glb(json, embedded))) {
        return 6;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (!loaded) {
        return 7;
    }

    const auto image = loaded.value().document.image_at(0U);
    if (!image ||
        !same_bytes(image.value().source_bytes, std::as_bytes(std::span{asymmetric_png}))) {
        return 7;
    }
    return 0;
}

[[nodiscard]] int test_jpeg_image(const TemporaryDirectory& temporary)
{
    const std::vector<std::byte> jpeg = decode_base64(jpeg_base64);
    const std::filesystem::path image_path = temporary.path() / "pixel.jpg";
    const std::filesystem::path gltf_path = temporary.path() / "jpeg.gltf";
    const std::string json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"supported.bin","byteLength":96}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],"images":[{"uri":"pixel.jpg"}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    if (!write_bytes(image_path, jpeg) || !write_text(gltf_path, json)) {
        return 8;
    }

    const auto loaded = elf3d::load_document(gltf_path.string());
    if (!loaded) {
        return 9;
    }

    const auto image = loaded.value().document.image_at(0U);
    return image && image.value().source_mime_type == elf3d::ModelImageMimeType::jpeg &&
                   same_bytes(image.value().source_bytes, jpeg)
               ? 0
               : 9;
}

using ContainerTest = int (*)(const TemporaryDirectory&);

[[nodiscard]] int test_containers_and_images(const TemporaryDirectory& temporary)
{
    constexpr std::array<ContainerTest, 5> tests{{
        test_data_uri_buffer,
        test_glb_container,
        test_implicit_scene,
        test_embedded_image,
        test_jpeg_image,
    }};
    for (const ContainerTest test : tests) {
        const int result = test(temporary);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

} // namespace elf3d::gltf::import_tests
