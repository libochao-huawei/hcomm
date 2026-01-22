set(src_list
    symmetric_memory.cc
    allgather_manager.cc
)

target_sources(hcomm PRIVATE
    ${src_list}
)