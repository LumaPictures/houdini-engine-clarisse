set(HAPI_MODULE_NAME "hapi")
set(HAPI_SERVER_NAME "hapi_server")

file(GLOB HEADERS "*.h*")
set(SOURCES
    hapi.cpp
    hapi_curve.cpp
    hapi_curve_from_memory.cpp
    hapi_mesh.cpp
    hapi_mesh_from_memory.cpp
    register_module.cpp)

gen_cmas(MODULE_CMAS_HAPI)
message(STATUS "MODULE_CMAS_HAPI ${MODULE_CMAS_HAPI}")

set(HAPI_INCLUDE_DIR "/Volumes/sv-dev01/soft/applications/houdini-15.0.179/toolkit/include/HAPI")
set(HAPI_LIB_DIR "/luma/soft/applications/houdini-15.0.179/dsolib")

include_directories(SYSTEM ${CLARISSE_INCLUDE_DIR})
link_directories(${CLARISSE_LIBRARY_DIR})

include_directories(SYSTEM ${HOUDINI_INCLUDE_DIR})
link_directories(${HOUDINI_LIB_DIR})

add_library(${HAPI_MODULE_NAME} SHARED ${SOURCES} ${MODULE_CMAS_HAPI})
set_target_properties(${HAPI_MODULE_NAME} PROPERTIES PREFIX "")

target_link_libraries(${HAPI_MODULE_NAME} HAPIL ix_core ix_gmath ix_geometry ix_of ix_point_cloud ix_module ix_poly ix_app ix_app ix_curve)

install(TARGETS ${HAPI_MODULE_NAME}
        LIBRARY DESTINATION modules)

add_executable(${HAPI_SERVER_NAME} hapi_server.cpp)
target_link_libraries(${HAPI_SERVER_NAME} HAPIL)

install(TARGETS ${HAPI_SERVER_NAME}
        RUNTIME DESTINATION bin)