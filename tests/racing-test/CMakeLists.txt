cmake_minimum_required(VERSION 3.18)
project(CocosRacingTest CXX C)

set(CMAKE_CXX_STANDARD 17)

# Pure C++ mode - no script engine

# Disable features not needed for this test
set(USE_VIDEO OFF)
set(USE_WEBVIEW OFF)
set(USE_PHYSICS_PHYSX OFF)

set(COCOS_X_PATH ${CMAKE_CURRENT_LIST_DIR}/../..)

# Include engine
include(${COCOS_X_PATH}/CMakeLists.txt)

set(TEST_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Game.h
    ${CMAKE_CURRENT_LIST_DIR}/Game.cpp
    ${CMAKE_CURRENT_LIST_DIR}/RaceWorld.h
    ${CMAKE_CURRENT_LIST_DIR}/RaceWorld.cpp
)

add_executable(CocosRacingTest ${TEST_SOURCES})

target_link_libraries(CocosRacingTest ${ENGINE_NAME})

target_include_directories(CocosRacingTest PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Windows-specific settings
if(MSVC)
    target_compile_definitions(CocosRacingTest PRIVATE
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
    target_compile_options(CocosRacingTest PRIVATE /MP /bigobj
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
        add_custom_command(TARGET CocosRacingTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${abs} $<TARGET_FILE_DIR:CocosRacingTest>/${filename}
        )
    endforeach()

    # Copy builtin resources and the car asset to output directory
    add_custom_command(TARGET CocosRacingTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${COCOS_X_PATH}/templates/wasm32/builtin-effects.json
            $<TARGET_FILE_DIR:CocosRacingTest>/builtin-effects.json
        COMMAND robocopy "${COCOS_X_PATH}/editor/assets/" "$<TARGET_FILE_DIR:CocosRacingTest>/" /E /XF *.meta /NFL /NDL /NJH /NJS || (exit 0)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_LIST_DIR}/assets
            $<TARGET_FILE_DIR:CocosRacingTest>/assets
    )
endif()
