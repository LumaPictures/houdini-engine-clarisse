CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(Clarisse)
add_definitions(-Wall -Wsign-compare -fPIC -std=c++11 -fno-rtti)

add_subdirectory(src)