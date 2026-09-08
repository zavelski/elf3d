# Check the generator before project() tries to locate an unsupported IDE.
if(CMAKE_GENERATOR MATCHES "^Visual Studio ([0-9]+) " AND CMAKE_MATCH_1 LESS 18)
    message(FATAL_ERROR
        "Elf3D requires Visual Studio 2026 or newer. Configure a new build directory "
        "with -G \"Visual Studio 18 2026\" -A x64 -T v145,host=x64.")
endif()

if(CMAKE_GENERATOR MATCHES "^Visual Studio " AND
   CMAKE_GENERATOR_TOOLSET MATCHES "^v([0-9]+)(,|$)" AND CMAKE_MATCH_1 LESS 145)
    message(FATAL_ERROR
        "Elf3D requires v145 or newer from Visual Studio 2026 or newer. "
        "Configure a new build directory with -T v145,host=x64.")
endif()

function(elf3d_check_windows_compiler)
    if(WIN32 AND (NOT MSVC OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"))
        message(FATAL_ERROR "Elf3D Windows builds require native MSVC from Visual Studio 2026 or newer.")
    endif()
    if(WIN32 AND MSVC AND MSVC_VERSION LESS 1950)
        message(FATAL_ERROR
            "Elf3D requires MSVC 19.50 or newer from Visual Studio 2026 or newer. "
            "Older toolsets are unsupported, including when installed in a newer IDE. "
            "Select v145 or newer and configure a new build directory.")
    endif()
    if(CMAKE_GENERATOR MATCHES "^Visual Studio ")
        if(NOT CMAKE_VS_PLATFORM_TOOLSET MATCHES "^v([0-9]+)$" OR CMAKE_MATCH_1 LESS 145)
            message(FATAL_ERROR "Elf3D requires the v145 platform toolset or newer. Configure a new build directory.")
        endif()
        message(STATUS
            "Elf3D toolchain: ${CMAKE_GENERATOR}; ${CMAKE_VS_PLATFORM_TOOLSET}; "
            "MSVC ${CMAKE_CXX_COMPILER_VERSION}; SDK ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}; "
            "instance ${CMAKE_GENERATOR_INSTANCE}; build tool ${CMAKE_MAKE_PROGRAM}")
    endif()
endfunction()
