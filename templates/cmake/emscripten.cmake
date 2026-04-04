# OFF = skip --preload-file (useful when postRun/checkStackCookie aborts during package unpack).
option(CC_WASM_PRELOAD_DATA "Pack RES_DIR/Resources/data with emcc --preload-file" ON)

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
    # Large engine + sync --preload-file unpacking runs in preRun; Emscripten's postRun
    # checkStackCookie often aborts here (stack canary) even with a multi-MB stack — disable
    # the cookie check for this target; use SAFE_HEAP / ASAN builds if you need memory audits.
    target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
        -sUSE_WEBGL2=1
        -sFULL_ES3=1
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=134217728
        -sSTACK_SIZE=33554432
        -sSTACK_OVERFLOW_CHECK=0
        -sENVIRONMENT=web
        --bind
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-sEXPORTED_FUNCTIONS=['_main','_wasmEditBoxOnInput','_wasmEditBoxOnConfirm','_wasmEditBoxOnComplete']"
    )

    if(CC_WASM_DEV_DIAGNOSTICS)
        target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
            -sASSERTIONS=1
            -gsource-map
            --profiling-funcs
        )
    else()
        target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
            -sASSERTIONS=0
        )
    endif()

    set_target_properties(${CC_EXECUTABLE_NAME} PROPERTIES
        SUFFIX ".html"
    )

    if(CC_WASM_PRELOAD_DATA AND EXISTS ${RES_DIR}/Resources/data)
        message(STATUS "WASM preload: ${RES_DIR}/Resources/data -> /data")
        target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
            --preload-file ${RES_DIR}/Resources/data@/data
        )
    elseif(NOT CC_WASM_PRELOAD_DATA AND EXISTS ${RES_DIR}/Resources/data)
        message(STATUS "WASM preload skipped (CC_WASM_PRELOAD_DATA=OFF); ${RES_DIR}/Resources/data not embedded")
    endif()

    # Preload the user entry script (main.js) from the project directory.
    if(EXISTS ${CC_PROJECT_DIR}/main.js)
        message(STATUS "WASM preload: ${CC_PROJECT_DIR}/main.js -> /data/main.js")
        target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
            --preload-file ${CC_PROJECT_DIR}/main.js@/data/main.js
        )
    endif()

    # Preload compiled builtin effects for standalone WASM rendering.
    if(EXISTS ${CC_PROJECT_DIR}/builtin-effects.json)
        message(STATUS "WASM preload: ${CC_PROJECT_DIR}/builtin-effects.json -> /data/builtin-effects.json")
        target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
            --preload-file ${CC_PROJECT_DIR}/builtin-effects.json@/data/builtin-effects.json
        )
    endif()

    cc_common_after_target(${CC_EXECUTABLE_NAME})
endmacro()
