cmake_minimum_required(VERSION 3.18)
project(CocosDemo CXX C)

set(CMAKE_CXX_STANDARD 17)

# Pure C++ mode — no script engine
set(USE_SE_V8 OFF)
set(USE_SE_SM OFF)
set(USE_SE_NAPI OFF)
set(USE_SE_JSVM OFF)
set(USE_SE_QJS OFF)
set(USE_SE_BROWSER OFF)

# Trim features irrelevant to 2D-UI demo target
set(USE_VIDEO OFF)
set(USE_WEBVIEW OFF)
set(USE_PHYSICS_PHYSX OFF)

set(COCOS_X_PATH ${CMAKE_CURRENT_LIST_DIR}/..)

include(${COCOS_X_PATH}/CMakeLists.txt)

set(DEMO_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/DemoGame.h
    ${CMAKE_CURRENT_LIST_DIR}/DemoGame.cpp
    ${CMAKE_CURRENT_LIST_DIR}/scenes/SceneRegistry.h
    ${CMAKE_CURRENT_LIST_DIR}/scenes/SceneRegistry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/scenes/EmptyScene.cpp
)

add_executable(CocosDemo ${DEMO_SOURCES})

target_link_libraries(CocosDemo ${ENGINE_NAME})

target_include_directories(CocosDemo PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

if(MSVC)
    target_compile_definitions(CocosDemo PRIVATE
        CC_STATIC
        WIN32 _WIN32 _WINDOWS
        NOMINMAX UNICODE _UNICODE _USE_MATH_DEFINES
        _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS
    )
    target_compile_options(CocosDemo PRIVATE /MP /bigobj
        /wd4018 /wd4221 /wd4244 /wd4267 /wd4305 /wd4309 /wd4819 /wd4996 /wd4098 /wd4099
    )

    file(GLOB WINDOWS_DLLS
        ${COCOS_X_PATH}/external/win64/libs/*.dll
        ${COCOS_X_PATH}/external/win64/libs/msvcr/*.dll
    )
    foreach(item ${WINDOWS_DLLS})
        get_filename_component(filename ${item} NAME)
        get_filename_component(abs ${item} ABSOLUTE)
        add_custom_command(TARGET CocosDemo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${abs} $<TARGET_FILE_DIR:CocosDemo>/${filename}
        )
    endforeach()

    add_custom_command(TARGET CocosDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${COCOS_X_PATH}/templates/wasm32/builtin-effects.json
            $<TARGET_FILE_DIR:CocosDemo>/builtin-effects.json
        COMMAND robocopy "${COCOS_X_PATH}/editor/assets/" "$<TARGET_FILE_DIR:CocosDemo>/" /E /XF *.meta /NFL /NDL /NJH /NJS || (exit 0)
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:CocosDemo>/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${COCOS_X_PATH}/editor/assets/default_fonts/builtin-freetype/OpenSans-Regular.ttf
            $<TARGET_FILE_DIR:CocosDemo>/fonts/OpenSans-Regular.ttf
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${COCOS_X_PATH}/editor/assets/default_fonts/builtin-freetype/OpenSans-Bold.ttf
            $<TARGET_FILE_DIR:CocosDemo>/fonts/OpenSans-Bold.ttf
    )
endif()
