target_sources(hccl_v2 PRIVATE
    net_instance.cc
    rank_graph.cc
)

if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
    net_instance.cc
    rank_graph.cc
)
endif()