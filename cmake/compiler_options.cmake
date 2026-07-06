function(elf3d_enable_warnings target_name)
    if(MSVC)
        target_compile_options(
            ${target_name}
            PRIVATE
                /W4
                /WX
                /permissive-
                /Zc:__cplusplus
                /Zc:preprocessor
        )
    else()
        target_compile_options(
            ${target_name}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
        )
    endif()
endfunction()

function(elf3d_link_object_libraries target_name)
    foreach(object_library IN LISTS ARGN)
        target_link_libraries(${target_name} PRIVATE ${object_library})
        target_sources(${target_name} PRIVATE $<TARGET_OBJECTS:${object_library}>)
    endforeach()
endfunction()
