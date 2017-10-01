CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(Clarisse)
find_package(Houdini)

set(CXX_SHARED_OPTIONS -Wall -Wsign-compare -fPIC -std=c++11 -fno-rtti)

link_directories(${HOUDINI_LIBRARY_DIR})

add_subdirectory(server)
# add_subdirectory(plugin)
