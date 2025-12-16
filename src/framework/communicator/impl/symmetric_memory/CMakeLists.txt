set(src_list
    symmetric_memory.cc
    symmetric_memory.h
)

target_sources(hcomm PRIVATE
    ${src_list}
)