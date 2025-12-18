set(src_list
    symmetric_memory.cc
    symmetric_memory_agent.cc
)

target_sources(hcomm PRIVATE
    ${src_list}
)