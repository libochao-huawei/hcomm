set(src_list
    ${CMAKE_CURRENT_SOURCE_DIR}/aclgraph_callback.cc
)

target_sources(hcomm PRIVATE
    ${src_list}
)

target_include_directories(hcomm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
