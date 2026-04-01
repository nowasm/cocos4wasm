
macro(cc_emscripten_before_target _target_name)
    if((NOT DEFINED CC_EXECUTABLE_NAME) OR "${CC_EXECUTABLE_NAME}" STREQUAL "")
        if(${APP_NAME} MATCHES "^[_0-9a-zA-Z-]+$")
            set(CC_EXECUTABLE_NAME ${APP_NAME})
        else()
            message(FATAL_ERROR "APP_NAME '${APP_NAME}' is invalid. Must match [_0-9a-zA-Z-]+")
        endif()
    endif()

    list(APPEND CC_PROJ_SOURCES
        ${CC_PROJECT_DIR}/main.cpp
    )

    list(APPEND CC_ALL_SOURCES ${CC_PROJ_SOURCES}
        ${CC_COMMON_SOURCES}
    )
    cc_common_before_target(${CC_EXECUTABLE_NAME})
endmacro()

macro(cc_emscripten_after_target _target_name)
    target_link_libraries(${CC_EXECUTABLE_NAME} ${ENGINE_NAME})

    target_include_directories(${CC_EXECUTABLE_NAME} PRIVATE
        ${CC_PROJECT_DIR}/../common/Classes
    )

    # Default new wasm projects to richer diagnostics in development builds.
    # Emscripten single-config generators often leave CMAKE_BUILD_TYPE empty,
    # so treat that case as a development build too.
    set(CC_WASM_DEV_DIAGNOSTICS OFF)
    if((NOT DEFINED CMAKE_BUILD_TYPE) OR "${CMAKE_BUILD_TYPE}" STREQUAL "" OR
       "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR
       "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
        set(CC_WASM_DEV_DIAGNOSTICS ON)
    endif()

    if(CC_WASM_DEV_DIAGNOSTICS)
        foreach(_cc_wasm_diag_target ${ENGINE_NAME} ${CC_EXECUTABLE_NAME})
            target_compile_options(${_cc_wasm_diag_target} PRIVATE
                -gsource-map
                --profiling-funcs
            )
        endforeach()
    endif()

    # Same entry as native: main.cpp uses START_PLATFORM -> UniversalPlatform::run
    # (cocos_main / Engine::tick via emscripten_set_main_loop in WasmPlatform).
    # Output .html so emcc emits a loadable page plus .js/.wasm (open via local HTTP server).
    target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
        -sUSE_WEBGL2=1
        -sFULL_ES3=1
        -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=5242880
        -sENVIRONMENT=web
        --bind
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    )

    if(CC_WASM_DEV_DIAGNOSTICS)
        target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
            -sASSERTIONS=2
            -gsource-map
            --profiling-funcs
        )
    endif()

    set_target_properties(${CC_EXECUTABLE_NAME} PROPERTIES
        SUFFIX ".html"
    )

    if(EXISTS ${RES_DIR}/Resources/data)
        target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
            --preload-file ${RES_DIR}/Resources/data@/data
        )
    endif()

    cc_common_after_target(${CC_EXECUTABLE_NAME})
endmacro()
