foreach(scenario IN LISTS ELF3D_FAILURE_SCENARIOS)
    execute_process(
        COMMAND "${ELF3D_FAILURE_EXECUTABLE}" ${ELF3D_FAILURE_ARGUMENT} "${scenario}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE diagnostic
        TIMEOUT 30
    )
    if(result STREQUAL "77")
        message("SKIP: graphics context unavailable for allocation failure validation")
        return()
    endif()
    if(scenario STREQUAL "unexpected")
        set(expected "Elf3D boundary encountered an unexpected exception")
    else()
        set(expected "Elf3D memory allocation failed")
    endif()
    if(result STREQUAL "0" OR result MATCHES "[Tt]imeout|[Tt]imed out" OR
       NOT diagnostic MATCHES "${expected}")
        message(FATAL_ERROR
            "${scenario} did not reach controlled fatal handling (${result}): ${output}${diagnostic}")
    endif()
endforeach()
