set(src_list
    symmetric_memory.cc
    symmetric_memory.h
    allgather_manager.cc
    allgather_manager.h
)

target_sources(hcomm PRIVATE
    ${src_list}
)