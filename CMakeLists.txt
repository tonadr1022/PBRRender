cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/vcpkg.cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(pbr_render VERSION 1.0)

add_compile_definitions(SRC_PATH="${CMAKE_SOURCE_DIR}/")

include_directories(src)
include_directories(dep)
add_subdirectory(src)

