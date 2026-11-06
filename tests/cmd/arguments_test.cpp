#include "arguments.h"

#include <array>
#include <string_view>
#include <vector>

static int valid_arguments_test()
{
    using namespace std::string_view_literals;
    const std::array valid{"--output"sv, "folder with spaces/result.glb"sv,
                           "--rotate"sv, "-1.25"sv,
                           "0"sv,        "2e1"sv,
                           "--input"sv,  "model.gltf"sv};
    const auto parsed = elf3d::cmd::parse_arguments(valid);
    if (!parsed || parsed.value().help || parsed.value().input != "model.gltf" ||
        parsed.value().output != "folder with spaces/result.glb" ||
        parsed.value().rotation_degrees != elf3d::Float3{-1.25F, 0.0F, 20.0F}) {
        return 1;
    }
    const std::array help{"--help"sv};
    const auto helped = elf3d::cmd::parse_arguments(help);
    if (!helped || !helped.value().help) {
        return 2;
    }
    return 0;
}

static int invalid_arguments_test()
{
    using namespace std::string_view_literals;
    const std::vector<std::vector<std::string_view>> invalid{
        {},
        {"--unknown"},
        {"--input", "a.glb", "--output", "b.glb", "--translate", "0", "0", "0"},
        {"--help", "--input", "a.glb"},
        {"--input"},
        {"--input", "a.obj", "--output", "b.glb", "--rotate", "0", "0", "0"},
        {"--input", "a.glb", "--output", "b.txt", "--rotate", "0", "0", "0"},
        {"--input", "a.glb", "--input", "b.glb"},
        {"--input", "a.glb", "--output", "b.glb"},
        {"--input", "a.glb", "--rotate", "0", "0", "0"},
        {"--output", "a.glb", "--rotate", "0", "0", "0"},
        {"--rotate", "0", "0"}};
    for (const auto& tokens : invalid) {
        if (elf3d::cmd::parse_arguments(tokens)) {
            return 3;
        }
    }
    for (const auto coordinate : {"nan"sv, "inf"sv, "1e1000"sv, "1,25"sv, "2x"sv, ""sv}) {
        const std::array tokens{"--input"sv,  "a.glb"sv,  "--output"sv, "b.glb"sv,
                                "--rotate"sv, coordinate, "0"sv,        "0"sv};
        if (elf3d::cmd::parse_arguments(tokens)) {
            return 4;
        }
    }
    const std::array duplicate{"--input"sv,  "a.glb"sv, "--output"sv, "b.glb"sv,
                               "--rotate"sv, "0"sv,     "0"sv,        "0"sv,
                               "--rotate"sv, "1"sv,     "2"sv,        "3"sv};
    return elf3d::cmd::parse_arguments(duplicate) ? 5 : 0;
}

int arguments_test()
{
    const int valid = valid_arguments_test();
    return valid != 0 ? valid : invalid_arguments_test();
}
