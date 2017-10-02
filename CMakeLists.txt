CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(Clarisse)
find_package(Houdini)

set(CXX_SHARED_OPTIONS -Wall -Wsign-compare -fPIC -std=c++11 -fno-rtti)

link_directories(${HOUDINI_LIBRARY_DIR})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_subdirectory(server)
add_subdirectory(plugin)