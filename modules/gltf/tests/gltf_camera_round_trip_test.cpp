#include <elf3d/model.h>

#include "../src/json_document.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::string_view fixture = R"json({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":42,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"}],
"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
"cameras":[{"name":"Authored camera","type":"perspective",
"perspective":{"yfov":0.5,"znear":0.25,"aspectRatio":1.75,
"extras":{"projection":"owned"}},"extras":{"camera":"owned"},
"extensions":{"EXT_camera_note":{"exact":9007199254740993}}}],
"nodes":[{"camera":0,"mesh":0},{"camera":0}],"scenes":[{"nodes":[0,1]}],"scene":0
})json";

[[nodiscard]] bool json_equal(std::string_view left, std::string_view right)
{
    const auto parsed_left = elf3d::gltf::json::parse(left);
    const auto parsed_right = elf3d::gltf::json::parse(right);
    if (!parsed_left || !parsed_right)
        return false;
    const auto normalized_left = elf3d::gltf::json::write(parsed_left.value());
    const auto normalized_right = elf3d::gltf::json::write(parsed_right.value());
    return normalized_left && normalized_right &&
           normalized_left.value() == normalized_right.value();
}

[[nodiscard]] int verify(const elf3d::Document& document)
{
    for (std::size_t index = 0; index < 2U; ++index) {
        const auto node = document.node_at(index);
        if (!node || !node.value().perspective_camera)
            return 101;
        const auto& camera = node.value().camera_properties;
        if (camera.name != "Authored camera" || !camera.infinite_far_plane ||
            !camera.aspect_ratio || *camera.aspect_ratio != 1.75F || !camera.metadata.extras_json ||
            !json_equal(*camera.metadata.extras_json, R"json({"camera":"owned"})json") ||
            !camera.projection_metadata.extras_json ||
            !json_equal(*camera.projection_metadata.extras_json,
                        R"json({"projection":"owned"})json") ||
            camera.metadata.extensions.size() != 1U ||
            camera.metadata.extensions.front().data.find("9007199254740993") ==
                std::string_view::npos)
            return 102;
    }
    return 0;
}

} // namespace

int elf3d_gltf_camera_round_trip_test()
{
    const auto directory = std::filesystem::path{ELF3D_TEST_BINARY_DIR} / "gltf_camera";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const auto path = directory / "source.gltf";
    {
        std::ofstream output{path};
        output << fixture;
        if (!output)
            return 103;
    }
    const auto loaded = elf3d::load_document(path.string());
    if (!loaded || verify(loaded.value().document) != 0)
        return 104;
    for (const std::string_view first : {"gltf", "glb"}) {
        const auto output = directory / ("camera." + std::string{first});
        if (!elf3d::save_document(output.string(), loaded.value().document.view()))
            return 105;
        const auto imported = elf3d::load_document(output.string());
        if (!imported || verify(imported.value().document) != 0)
            return 106;
        for (const std::string_view second : {"gltf", "glb"}) {
            const auto again =
                directory / ("again-" + std::string{first} + "." + std::string{second});
            if (!elf3d::save_document(again.string(), imported.value().document.view()))
                return 107;
            const auto checked = elf3d::load_document(again.string());
            if (!checked || verify(checked.value().document) != 0)
                return 108;
        }
    }
    std::filesystem::remove_all(directory, error);
    return 0;
}
