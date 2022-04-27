cmake_minimum_required(VERSION 3.13)
project(MineClone)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)

include(Resources)

add_subdirectory("Libraries")
add_subdirectory("Client")
add_subdirectory("Executable")
