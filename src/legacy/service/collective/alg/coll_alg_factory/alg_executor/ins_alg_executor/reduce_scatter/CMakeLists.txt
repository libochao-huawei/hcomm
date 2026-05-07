target_sources(hccl_v2 PRIVATE
        ins_reduce_scatter_parallel_executor.cc
        ins_reduce_scatter_sole_executor.cc
        ins_v2_reduce_scatter_sole_executor.cc
        ins_v2_reduce_scatter_v_sole_executor.cc
)

if(NOT BUILD_OPEN_PROJECT OR DEVICE_MODE)
target_sources(ccl_kernel PRIVATE
        ins_reduce_scatter_parallel_executor.cc
        ins_reduce_scatter_sole_executor.cc
        ins_v2_reduce_scatter_sole_executor.cc
        ins_v2_reduce_scatter_v_sole_executor.cc
)
endif()