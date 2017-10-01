set(HAPI_MODULE_NAME "hapi")

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

include_directories(SYSTEM ${CLARISSE_INCLUDE_DIR})
link_directories(${CLARISSE_LIBRARY_DIR})

include_directories(SYSTEM ${HOUDINI_INCLUDE_DIR})

add_library(${HAPI_MODULE_NAME} SHARED ${SOURCES} ${MODULE_CMAS_HAPI})
set_target_properties(${HAPI_MODULE_NAME} PROPERTIES PREFIX "")
target_compile_options(${HAPI_MODULE_NAME} PRIVATE ${CXX_SHARED_OPTIONS})

target_link_libraries(${HAPI_MODULE_NAME} HAPIL ix_core ix_gmath ix_geometry ix_of ix_point_cloud ix_module ix_poly ix_app ix_app ix_curve)
