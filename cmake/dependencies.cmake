set(ELF3D_GLM_VERSION "1.0.3")
set(ELF3D_GLM_COMMIT_SHA "8d1fd52e5ab5590e2c81768ace50c72bae28f2ed")
set(ELF3D_GLAD_VERSION "v2.0.8")
set(ELF3D_GLAD_COMMIT_SHA "73db193f853e2ee079bf3ca8a64aa2eaf6459043")
set(ELF3D_IMGUI_BRANCH "docking")
set(ELF3D_IMGUI_COMMIT_SHA "036bf939b6f8d74ad76bcf926b757c56e68c54ff")
set(ELF3D_GLFW_VERSION "3.4")
set(ELF3D_GLFW_COMMIT_SHA "a74efa0d5628b74adc0426af4c5710e287fa7c2c")
set(ELF3D_CGLTF_VERSION "1.15")
set(ELF3D_CGLTF_COMMIT_SHA "360db1a95480fe102ae9c69b27c5d101167ff5ba")
set(ELF3D_ZLIB_VERSION "1.3.1")
set(ELF3D_LIBPNG_VERSION "1.6.58")
set(ELF3D_LIBPNG_ARCHIVE_SHA256 "28eb403f51f0f7405249132cecfe82ea5c0ef97f1b32c5a65828814ae0d34775")
set(ELF3D_LIBJPEG_VERSION "10")
set(ELF3D_LIBJPEG_ARCHIVE_SHA256 "8b9eaa13242690ebd03e1728ab1edf97a81a78ed6e83624d493655f31ac95ab5")

set(ELF3D_THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party")
set(cgltf_SOURCE_DIR "${ELF3D_THIRD_PARTY_DIR}/cgltf")
set(zlib_SOURCE_DIR "${ELF3D_THIRD_PARTY_DIR}/zlib")
set(libpng_SOURCE_DIR "${ELF3D_THIRD_PARTY_DIR}/png")
set(libjpeg_SOURCE_DIR "${ELF3D_THIRD_PARTY_DIR}/jpeg")
set(imgui_SOURCE_DIR "${ELF3D_THIRD_PARTY_DIR}/imgui")

add_library(elf3d_third_party_glm INTERFACE)
add_library(glm::glm ALIAS elf3d_third_party_glm)
target_include_directories(
    elf3d_third_party_glm
    SYSTEM INTERFACE
        ${ELF3D_THIRD_PARTY_DIR}/glm
)
set_target_properties(
    elf3d_third_party_glm
    PROPERTIES
        FOLDER "Third Party"
)

if(ELF3D_BUILD_ENGINE)
    add_library(
        elf3d_third_party_glad
        STATIC
            ${ELF3D_THIRD_PARTY_DIR}/glad/include/glad/gl.h
            ${ELF3D_THIRD_PARTY_DIR}/glad/include/KHR/khrplatform.h
            ${ELF3D_THIRD_PARTY_DIR}/glad/src/gl.c
    )
    add_library(elf3d::third_party_glad ALIAS elf3d_third_party_glad)
    target_compile_features(elf3d_third_party_glad PRIVATE c_std_11)
    target_include_directories(
        elf3d_third_party_glad
        SYSTEM PUBLIC
            ${ELF3D_THIRD_PARTY_DIR}/glad/include
    )
    set_target_properties(
        elf3d_third_party_glad
        PROPERTIES
            FOLDER "Third Party"
            POSITION_INDEPENDENT_CODE ON
    )
endif()

add_library(
    elf3d_third_party_zlib
    STATIC
        ${zlib_SOURCE_DIR}/adler32.c
        ${zlib_SOURCE_DIR}/compress.c
        ${zlib_SOURCE_DIR}/crc32.c
        ${zlib_SOURCE_DIR}/deflate.c
        ${zlib_SOURCE_DIR}/infback.c
        ${zlib_SOURCE_DIR}/inffast.c
        ${zlib_SOURCE_DIR}/inflate.c
        ${zlib_SOURCE_DIR}/inftrees.c
        ${zlib_SOURCE_DIR}/trees.c
        ${zlib_SOURCE_DIR}/uncompr.c
        ${zlib_SOURCE_DIR}/zutil.c
)
target_compile_features(elf3d_third_party_zlib PRIVATE c_std_11)
target_compile_definitions(
    elf3d_third_party_zlib
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)
target_include_directories(elf3d_third_party_zlib SYSTEM PUBLIC ${zlib_SOURCE_DIR})
set_target_properties(
    elf3d_third_party_zlib
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)

add_library(
    elf3d_third_party_png
    STATIC
        ${libpng_SOURCE_DIR}/png.c
        ${libpng_SOURCE_DIR}/pngerror.c
        ${libpng_SOURCE_DIR}/pngget.c
        ${libpng_SOURCE_DIR}/pngmem.c
        ${libpng_SOURCE_DIR}/pngpread.c
        ${libpng_SOURCE_DIR}/pngread.c
        ${libpng_SOURCE_DIR}/pngrio.c
        ${libpng_SOURCE_DIR}/pngrtran.c
        ${libpng_SOURCE_DIR}/pngrutil.c
        ${libpng_SOURCE_DIR}/pngset.c
        ${libpng_SOURCE_DIR}/pngtrans.c
        ${libpng_SOURCE_DIR}/pngwio.c
        ${libpng_SOURCE_DIR}/pngwrite.c
        ${libpng_SOURCE_DIR}/pngwtran.c
        ${libpng_SOURCE_DIR}/pngwutil.c
)
target_compile_features(elf3d_third_party_png PRIVATE c_std_11)
target_compile_definitions(
    elf3d_third_party_png
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)
target_include_directories(elf3d_third_party_png SYSTEM PUBLIC ${libpng_SOURCE_DIR})
target_link_libraries(elf3d_third_party_png PRIVATE elf3d_third_party_zlib)
set_target_properties(
    elf3d_third_party_png
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)

add_library(
    elf3d_third_party_jpeg
    STATIC
        ${libjpeg_SOURCE_DIR}/jaricom.c
        ${libjpeg_SOURCE_DIR}/jcapimin.c
        ${libjpeg_SOURCE_DIR}/jcapistd.c
        ${libjpeg_SOURCE_DIR}/jcarith.c
        ${libjpeg_SOURCE_DIR}/jccoefct.c
        ${libjpeg_SOURCE_DIR}/jccolor.c
        ${libjpeg_SOURCE_DIR}/jcdctmgr.c
        ${libjpeg_SOURCE_DIR}/jchuff.c
        ${libjpeg_SOURCE_DIR}/jcinit.c
        ${libjpeg_SOURCE_DIR}/jcmainct.c
        ${libjpeg_SOURCE_DIR}/jcmarker.c
        ${libjpeg_SOURCE_DIR}/jcmaster.c
        ${libjpeg_SOURCE_DIR}/jcomapi.c
        ${libjpeg_SOURCE_DIR}/jcparam.c
        ${libjpeg_SOURCE_DIR}/jcprepct.c
        ${libjpeg_SOURCE_DIR}/jcsample.c
        ${libjpeg_SOURCE_DIR}/jctrans.c
        ${libjpeg_SOURCE_DIR}/jdapimin.c
        ${libjpeg_SOURCE_DIR}/jdapistd.c
        ${libjpeg_SOURCE_DIR}/jdarith.c
        ${libjpeg_SOURCE_DIR}/jdatadst.c
        ${libjpeg_SOURCE_DIR}/jdatasrc.c
        ${libjpeg_SOURCE_DIR}/jdcoefct.c
        ${libjpeg_SOURCE_DIR}/jdcolor.c
        ${libjpeg_SOURCE_DIR}/jddctmgr.c
        ${libjpeg_SOURCE_DIR}/jdhuff.c
        ${libjpeg_SOURCE_DIR}/jdinput.c
        ${libjpeg_SOURCE_DIR}/jdmainct.c
        ${libjpeg_SOURCE_DIR}/jdmarker.c
        ${libjpeg_SOURCE_DIR}/jdmaster.c
        ${libjpeg_SOURCE_DIR}/jdmerge.c
        ${libjpeg_SOURCE_DIR}/jdpostct.c
        ${libjpeg_SOURCE_DIR}/jdsample.c
        ${libjpeg_SOURCE_DIR}/jdtrans.c
        ${libjpeg_SOURCE_DIR}/jerror.c
        ${libjpeg_SOURCE_DIR}/jfdctflt.c
        ${libjpeg_SOURCE_DIR}/jfdctfst.c
        ${libjpeg_SOURCE_DIR}/jfdctint.c
        ${libjpeg_SOURCE_DIR}/jidctflt.c
        ${libjpeg_SOURCE_DIR}/jidctfst.c
        ${libjpeg_SOURCE_DIR}/jidctint.c
        ${libjpeg_SOURCE_DIR}/jmemmgr.c
        ${libjpeg_SOURCE_DIR}/jmemnobs.c
        ${libjpeg_SOURCE_DIR}/jquant1.c
        ${libjpeg_SOURCE_DIR}/jquant2.c
        ${libjpeg_SOURCE_DIR}/jutils.c
)
target_compile_features(elf3d_third_party_jpeg PRIVATE c_std_11)
target_compile_definitions(
    elf3d_third_party_jpeg
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)
target_include_directories(elf3d_third_party_jpeg SYSTEM PUBLIC ${libjpeg_SOURCE_DIR})
set_target_properties(
    elf3d_third_party_jpeg
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)

add_library(
    elf3d_third_party_cgltf
    STATIC
        ${PROJECT_SOURCE_DIR}/modules/gltf/src/cgltf_implementation.cpp
)
target_compile_features(elf3d_third_party_cgltf PRIVATE cxx_std_20)
target_include_directories(elf3d_third_party_cgltf SYSTEM PRIVATE ${cgltf_SOURCE_DIR})
set_target_properties(
    elf3d_third_party_cgltf
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)

if(NOT ELF3D_BUILD_VIEWER)
    return()
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${ELF3D_THIRD_PARTY_DIR}/glfw
    ${CMAKE_BINARY_DIR}/third_party/glfw
    EXCLUDE_FROM_ALL
)

find_package(OpenGL REQUIRED)

add_library(
    elf3d_third_party_imgui
    STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(elf3d::third_party_imgui ALIAS elf3d_third_party_imgui)

target_compile_features(elf3d_third_party_imgui PUBLIC cxx_std_20)
target_include_directories(
    elf3d_third_party_imgui
    SYSTEM PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(
    elf3d_third_party_imgui
    PUBLIC
        glfw
        OpenGL::GL
)
set_target_properties(
    elf3d_third_party_imgui
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)

if(TARGET glfw)
    set_target_properties(glfw PROPERTIES FOLDER "Third Party")
endif()
