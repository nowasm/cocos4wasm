cmake_minimum_required(VERSION 3.18)
project(CocosCppTest CXX C)

set(CMAKE_CXX_STANDARD 17)

# Pure C++ mode - no script engine
set(USE_SE_V8 OFF)
set(USE_SE_SM OFF)
set(USE_SE_NAPI OFF)
set(USE_SE_JSVM OFF)
set(USE_SE_QJS OFF)
set(USE_SE_BROWSER OFF)

# Disable features not needed for basic test
set(USE_VIDEO OFF)
set(USE_WEBVIEW OFF)
set(USE_PHYSICS_PHYSX OFF)

set(COCOS_X_PATH ${CMAKE_CURRENT_LIST_DIR}/../..)

# Include engine
include(${COCOS_X_PATH}/CMakeLists.txt)

# Test executable sources
set(TEST_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Game.h
    ${CMAKE_CURRENT_LIST_DIR}/Game.cpp
    ${CMAKE_CURRENT_LIST_DIR}/TestScene.h
    ${CMAKE_CURRENT_LIST_DIR}/tests/TestShapes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tests/TestMaterials.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tests/TestLighting.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tests/TestTransform.cpp
)

add_executable(CocosCppTest ${TEST_SOURCES})

target_link_libraries(CocosCppTest ${ENGINE_NAME})

target_include_directories(CocosCppTest PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Windows-specific settings
if(MSVC)
    target_compile_definitions(CocosCppTest PRIVATE
        CC_STATIC
        WIN32
        _WIN32
        _WINDOWS
        NOMINMAX
        UNICODE
        _UNICODE
        _USE_MATH_DEFINES
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
    )
    target_compile_options(CocosCppTest PRIVATE /MP /bigobj
        /wd4018 /wd4221 /wd4244 /wd4267 /wd4305 /wd4309 /wd4819 /wd4996 /wd4098 /wd4099
    )

    # Copy DLLs to output directory
    file(GLOB WINDOWS_DLLS
        ${COCOS_X_PATH}/external/win64/libs/*.dll
        ${COCOS_X_PATH}/external/win64/libs/msvcr/*.dll
    )
    foreach(item ${WINDOWS_DLLS})
        get_filename_component(filename ${item} NAME)
        get_filename_component(abs ${item} ABSOLUTE)
        add_custom_command(TARGET CocosCppTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${abs} $<TARGET_FILE_DIR:CocosCppTest>/${filename}
        )
    endforeach()

    # Copy builtin resources to output directory
    add_custom_command(TARGET CocosCppTest POST_BUILD
        # builtin-effects.json (shaders)
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${COCOS_X_PATH}/templates/wasm32/builtin-effects.json
            $<TARGET_FILE_DIR:CocosCppTest>/builtin-effects.json
        # builtin fonts
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:CocosCppTest>/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${COCOS_X_PATH}/editor/assets/default_fonts/builtin-freetype/OpenSans-Regular.ttf
            $<TARGET_FILE_DIR:CocosCppTest>/fonts/OpenSans-Regular.ttf
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${COCOS_X_PATH}/editor/assets/default_fonts/builtin-freetype/OpenSans-Bold.ttf
            $<TARGET_FILE_DIR:CocosCppTest>/fonts/OpenSans-Bold.ttf
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${COCOS_X_PATH}/editor/assets/default_fonts/builtin-freetype/OpenSans-Italic.ttf
            $<TARGET_FILE_DIR:CocosCppTest>/fonts/OpenSans-Italic.ttf
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${COCOS_X_PATH}/editor/assets/default_fonts/builtin-freetype/OpenSans-BoldItalic.ttf
            $<TARGET_FILE_DIR:CocosCppTest>/fonts/OpenSans-BoldItalic.ttf
    )
endif()
