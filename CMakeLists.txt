cmake_minimum_required(VERSION 2.8)
project(tasmota-shutter-control)

set(CMAKE_BUILD_TYPE "Debug")

add_definitions(/D__STDC_FORMAT_MACROS)
add_definitions(/DUNICODE)
add_definitions("/DCXXOPTS_NO_RTTI")

set(SOURCES
    src/tasmotashuttercontrol.cpp
)

include_directories(src)
include_directories(src/3rdparty)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
target_link_libraries(${CMAKE_PROJECT_NAME})
