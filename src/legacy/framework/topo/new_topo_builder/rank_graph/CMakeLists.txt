target_sources(hccl_v2 PRIVATE
    net_instance.cc
    rank_graph.cc
)

if(TARGET ccl_kernel)
target_sources(ccl_kernel PRIVATE
    net_instance.cc
    rank_graph.cc
)
endif()