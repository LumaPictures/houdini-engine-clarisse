set(HAPI_MODULE_NAME "hapi")

file(GLOB SOURCES "*.cpp")

gen_cmas(MODULE_CMAS_HAPI)
message(STATUS "MODULE_CMAS_HAPI ${MODULE_CMAS_HAPI}")

set(HAPI_INCLUDE_DIR "/Volumes/sv-dev01/soft/applications/houdini-15.0.179/toolkit/include/HAPI")
set(HAPI_LIB_DIR "/luma/soft/applications/houdini-15.0.179/dsolib")

include_directories(SYSTEM ${CLARISSE_INCLUDE_DIR})
link_directories(${CLARISSE_LIBRARY_DIR})

include_directories(SYSTEM ${HAPI_INCLUDE_DIR})
link_directories(${HAPI_LIB_DIR})

add_library(${HAPI_MODULE_NAME} SHARED ${SOURCES} ${MODULE_CMAS_HAPI})
set_target_properties(${HAPI_MODULE_NAME} PROPERTIES PREFIX "")

target_link_libraries(${HAPI_MODULE_NAME} HAPIL ix_core ix_gmath ix_geometry ix_of ix_point_cloud ix_module ix_poly ix_app ix_app ix_curve)

install(TARGETS ${HAPI_MODULE_NAME}
        LIBRARY DESTINATION modules)

add_executable("hapi_server" hapi_server.cpp)
target_link_libraries("hapi_server" HAPIL)

install(TARGETS "hapi_server"
        RUNTIME DESTINATION bin)