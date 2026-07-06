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
    elf3d_image
    OBJECT
        src/image_decoder.cpp
)
add_library(elf3d::image ALIAS elf3d_image)

target_compile_features(elf3d_image PUBLIC cxx_std_20)
target_sources(
    elf3d_image
    PUBLIC
        FILE_SET elf3d_image_modules TYPE CXX_MODULES
        FILES
            src/elf.image.cppm
)
target_include_directories(
    elf3d_image
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(
    elf3d_image
    PUBLIC
        elf3d_core
    PRIVATE
        elf3d_third_party_png
        elf3d_third_party_jpeg
)
set_target_properties(
    elf3d_image
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_image)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
