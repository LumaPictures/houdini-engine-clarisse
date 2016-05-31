# - Clarisse finder module
# This module searches for a valid Clarisse instalation.

find_path(CLARISSE_LIBRARY_DIR libix_of.so
    PATHS $ENV{CLARISSE_HOME}
    DOC "Clarisse library path")

find_path(CLARISSE_INCLUDE_DIR app.h
    PATHS $ENV{CLARISSE_HOME}/sdk/include
    DOC "Clarisse include path")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Clarisse DEFAULT_MSG
    CLARISSE_LIBRARY_DIR CLARISSE_INCLUDE_DIR)

function(set_cid_includes input_dir)
    if(CID_INCLUDES)
        set(CID_INCLUDES "${CID_INCLUDES} ${input_dir}" PARENT_SCOPE)
    else()
        set(CID_INCLUDES "${input_dir}" PARENT_SCOPE)
    endif()
endfunction()

function(gen_cmas target_env)    
    set(LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${CLARISSE_HOME}")
    set(PATH "${PATH}:${CLARISSE_HOME}")
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    file(GLOB CIDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cid)
    foreach(current_cid ${CIDS})
        string(REPLACE ".cid" "" current_cid_raw ${current_cid})
        set(OUTPUT_CMA "${CMAKE_CURRENT_BINARY_DIR}/${current_cid_raw}.cma")
        set(INPUT_CID "${CMAKE_CURRENT_SOURCE_DIR}/${current_cid}")
        list(APPEND TEMP_CMA_LIST "${OUTPUT_CMA}")
        if(CMA_INCLUDES)
            add_custom_command(OUTPUT ${OUTPUT_CMA}
                               PRE_BUILD
                               COMMAND cmagen ${INPUT_CID} -output ${OUTPUT_CMA} -search_path ${CMA_INCLUDES}
                               DEPENDS ${INPUT_CID})
        else()
            add_custom_command(OUTPUT ${OUTPUT_CMA}
                               PRE_BUILD
                               COMMAND cmagen ${INPUT_CID} -output ${OUTPUT_CMA}
                               DEPENDS ${INPUT_CID})
        endif()
        message(STATUS "Generating ${OUTPUT_CMA} from ${INPUT_CID}")
    endforeach()
    set(${target_env} ${TEMP_CMA_LIST} PARENT_SCOPE)
endfunction()
