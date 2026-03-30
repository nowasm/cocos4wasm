cmake_minimum_required(VERSION 3.8)

set(APP_NAME "CocosGame" CACHE STRING "Project Name")

project(${APP_NAME} CXX)

set(CC_PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR})
set(CC_PROJ_SOURCES)
set(CC_COMMON_SOURCES)
set(CC_ALL_SOURCES)
include(${CC_PROJECT_DIR}/../common/CMakeLists.txt)
include(${CC_PROJECT_DIR}/../cmake/emscripten.cmake)
set(EXECUTABLE_NAME ${APP_NAME})

cc_emscripten_before_target(${APP_NAME})
add_executable(${APP_NAME} ${CC_ALL_SOURCES})
cc_emscripten_after_target(${APP_NAME})
