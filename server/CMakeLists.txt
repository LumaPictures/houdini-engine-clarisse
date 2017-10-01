set(HAPI_SERVER_NAME "hapi_server")

file(GLOB SOURCE *.cpp)

install(TARGETS ${HAPI_MODULE_NAME}
        LIBRARY DESTINATION modules)

add_executable(${HAPI_SERVER_NAME} ${SOURCE})
target_include_directories(${HAPI_SERVER_NAME} SYSTEM PRIVATE ${HOUDINI_INCLUDE_DIR})
target_compile_options(${HAPI_SERVER_NAME} PRIVATE ${CXX_SHARED_OPTIONS})
target_link_libraries(${HAPI_SERVER_NAME} HAPIL)

install(TARGETS ${HAPI_SERVER_NAME}
        RUNTIME DESTINATION bin)