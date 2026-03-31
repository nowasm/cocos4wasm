set(BINARY ${CMAKE_PROJECT_NAME})

file(GLOB_RECURSE SOURCES LIST_DIRECTORIES true *.h *.cpp)

# Ensure CMake re-runs when new source files are added
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR})

add_executable(${BINARY} ${SOURCES})

add_test(NAME ${BINARY} COMMAND ${BINARY})

target_link_libraries(${BINARY} PUBLIC gtest ${ENGINE_NAME})
target_include_directories(${BINARY} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../..)

if(EMSCRIPTEN)
    # Match the app wasm template: keep source maps and named wasm functions
    # in development-oriented builds so browser errors are easier to debug.
    set(CC_WASM_DEV_DIAGNOSTICS OFF)
    if((NOT DEFINED CMAKE_BUILD_TYPE) OR "${CMAKE_BUILD_TYPE}" STREQUAL "" OR
       "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR
       "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
        set(CC_WASM_DEV_DIAGNOSTICS ON)
    endif()

    if(CC_WASM_DEV_DIAGNOSTICS)
        foreach(_cc_wasm_diag_target ${ENGINE_NAME} ${BINARY})
            target_compile_options(${_cc_wasm_diag_target} PRIVATE
                -gsource-map
            )
        endforeach()
    endif()

    # Allow heap to grow, export main, enable printf output to browser console,
    # and link the virtual filesystem (needed by gtest's internal file I/O).
    target_link_options(${BINARY} PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sEXIT_RUNTIME=1
        -sENVIRONMENT=web,node
        -sSTACK_SIZE=1048576
        -sASSERTIONS=1
    )

    if(CC_WASM_DEV_DIAGNOSTICS)
        target_link_options(${BINARY} PRIVATE
            -gsource-map
            --profiling-funcs
        )
    endif()
elseif(MSVC)
    foreach(item ${WINDOWS_DLLS})
        get_filename_component(filename ${item} NAME)
        get_filename_component(abs ${item} ABSOLUTE)
        add_custom_command(TARGET ${BINARY} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${abs} $<TARGET_FILE_DIR:${BINARY}>/${filename}
        )
    endforeach()
    foreach(item ${V8_DLLS})
        get_filename_component(filename ${item} NAME)
        add_custom_command(TARGET ${BINARY} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${V8_DIR}/$<IF:$<BOOL:$<CONFIG:RELEASE>>,Release,Debug>/${filename} $<TARGET_FILE_DIR:${BINARY}>/${filename}
        )
    endforeach()
    target_link_options(${BINARY} PRIVATE /SUBSYSTEM:CONSOLE)
endif()