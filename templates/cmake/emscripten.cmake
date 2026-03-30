
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

    target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
        -sUSE_WEBGL2=1
        -sFULL_ES3=1
        -sALLOW_MEMORY_GROWTH=1
        --bind
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    )

    set_target_properties(${CC_EXECUTABLE_NAME} PROPERTIES
        SUFFIX ".js"
    )

    if(EXISTS ${RES_DIR}/data)
        target_link_options(${CC_EXECUTABLE_NAME} PRIVATE
            --preload-file ${RES_DIR}/data@/data
        )
    endif()

    cc_common_after_target(${CC_EXECUTABLE_NAME})
endmacro()
