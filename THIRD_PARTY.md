# Third-Party Software

Dependencies are fetched from their official repositories by CMake FetchContent
and pinned to reproducible revisions. Project warning-as-error settings are not
applied to their implementation files.

Elf3D original source code is licensed under the MIT License in the root
`LICENSE` file. Third-party components listed here remain governed by their own
licenses and are not relicensed as Elf3D source code.

## Dear ImGui

- Official repository: <https://github.com/ocornut/imgui.git>
- Source branch: `docking`
- Pinned commit: `036bf939b6f8d74ad76bcf926b757c56e68c54ff`
- Revision resolved: 2026-06-22
- License: MIT
- Integration: CMake FetchContent and dedicated `elf3d_third_party_imgui` static target
- Compiled core sources: `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
  `imgui_widgets.cpp`, and `imgui_demo.cpp`
- Compiled backends: `backends/imgui_impl_glfw.cpp` and
  `backends/imgui_impl_opengl3.cpp`

The upstream license notice is preserved in `third_party/licenses/imgui-LICENSE.txt`.

## GLFW

- Official repository: <https://github.com/glfw/glfw.git>
- Stable release: `3.4`
- Pinned commit: `a74efa0d5628b74adc0426af4c5710e287fa7c2c`
- License: zlib/libpng
- Integration: CMake FetchContent with examples, tests, documentation, and install
  rules disabled

The upstream license notice is preserved in `third_party/licenses/glfw-LICENSE.md`.

## GLM

- Official repository: <https://github.com/g-truc/glm.git>
- Stable release: `1.0.3`
- Pinned commit: `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed`
- Release inspected: 2026-06-22
- License: Happy Bunny License or MIT License
- Integration: CMake FetchContent using the official exact-commit archive with SHA-256 verification
- Scope: implementation dependency of the internal `elf3d_math` target only

The upstream license notice is preserved in `third_party/licenses/glm-copying.txt`.

## GLAD

- Official repository: <https://github.com/Dav1dde/glad.git>
- Stable release: `v2.0.8`
- Pinned commit: `73db193f853e2ee079bf3ca8a64aa2eaf6459043`
- Release inspected: 2026-06-22
- License: MIT; generated Khronos-derived files retain their applicable notices
- Generated API: OpenGL `4.1`, `core` profile, zero extensions
- Integration: reproducibly generated C loader checked into `third_party/glad`
- Scope: private dependency of `elf3d_backend_opengl` only

The generator was invoked with `--api gl:core=4.1 --extensions " " --reproducible c`.
The upstream license notice is preserved in `third_party/licenses/glad-LICENSE.txt`.

## cgltf

- Official repository: <https://github.com/jkuhlmann/cgltf.git>
- Stable release: `v1.15`
- Pinned commit: `360db1a95480fe102ae9c69b27c5d101167ff5ba`
- Release inspected: 2026-06-22
- License: MIT
- Integration: CMake FetchContent plus a dedicated `elf3d_third_party_cgltf`
  static target
- Scope: private dependency of the internal `elf3d_gltf` target only

`CGLTF_IMPLEMENTATION` is defined in exactly one private project source file,
`modules/gltf/src/cgltf_implementation.cpp`. cgltf headers and types do not
propagate through Elf3D public interfaces. The upstream license notice is
preserved in `third_party/licenses/cgltf-LICENSE.txt`.

## stb

- Official repository: <https://github.com/nothings/stb.git>
- Pinned commit: `31c1ad37456438565541f4919958214b6e762fb4`
- Revision inspected: 2026-06-22 (commit dated 2026-04-15)
- License: MIT or public domain, at the user's choice
- Integration: CMake FetchContent plus a dedicated `elf3d_third_party_stb` static target
- Enabled decoder formats: PNG and JPEG only
- Scope: private implementation dependency of the internal `elf3d_image` target only

`STB_IMAGE_IMPLEMENTATION` is defined in exactly one private project source file,
`modules/image/src/stb_image_implementation.cpp`. The same file limits stb_image
to PNG and JPEG. stb headers and types do not propagate through Elf3D public
interfaces. The upstream license notice is preserved in
`third_party/licenses/stb-LICENSE.txt`.

## Droid Sans

- Official source: <https://android.googlesource.com/platform/frameworks/base/+/dba35c0/data/fonts/DroidSans.ttf>
- Source revision inspected: `dba35c0`
- Asset path: `apps/viewer/assets/font/DroidSans.ttf`
- SHA-256: `4e2371bc0e4cf6983342e150412f140da79d674c9be0b56458401f581072ecd3`
- License: Apache License 2.0
- Integration: copied binary runtime font asset for `elf3d_viewer`
- Scope: viewer UI font only; the Dear ImGui integration falls back to the
  default font when the asset is unavailable

The copied `DroidSans.ttf` file was verified to be byte-identical to the AOSP
font at the revision above. The upstream Apache License 2.0 notice is preserved
in `third_party/licenses/droidsans-APACHE-2.0.txt`.
