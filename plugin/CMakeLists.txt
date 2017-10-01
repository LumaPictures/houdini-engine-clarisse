set(HAPI_MODULE_NAME "hapi")

file(GLOB HEADERS "*.h*")
file(GLOB SOURCES "*.cpp")

gen_cmas(MODULE_CMAS_HAPI)

link_directories(${CLARISSE_LIBRARY_DIR})

add_library(${HAPI_MODULE_NAME} SHARED ${SOURCES} ${HEADERS} ${MODULE_CMAS_HAPI})
set_target_properties(${HAPI_MODULE_NAME} PROPERTIES PREFIX "")
target_compile_options(${HAPI_MODULE_NAME} PRIVATE ${CXX_SHARED_OPTIONS})
target_include_directories(${HAPI_MODULE_NAME} PRIVATE ${CLARISSE_INCLUDE_DIR})
target_include_directories(${HAPI_MODULE_NAME} SYSTEM PRIVATE ${HOUDINI_INCLUDE_DIR})
target_link_libraries(${HAPI_MODULE_NAME} HAPIL ix_core ix_gmath ix_geometry ix_of ix_point_cloud ix_module ix_poly ix_app ix_app ix_curve)

install(TARGETS ${HAPI_MODULE_NAME}
        LIBRARY DESTINATION modules)