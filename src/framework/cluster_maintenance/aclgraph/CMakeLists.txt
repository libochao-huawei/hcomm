set(src_list
    ${CMAKE_CURRENT_SOURCE_DIR}/aclgraph_callback.cc
)

if(TARGET hcomm)
    target_sources(hcomm PRIVATE
        ${src_list}
    )
endif()
