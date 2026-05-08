set(src_list
    symmetric_memory.cc
    symmetric_memory_agent.cc
)

if(TARGET hcomm)
    target_sources(hcomm PRIVATE
        ${src_list}
    )
endif()