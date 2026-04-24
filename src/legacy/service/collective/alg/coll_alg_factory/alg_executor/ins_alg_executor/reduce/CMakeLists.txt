target_sources(hccl_v2 PRIVATE
        ins_reduce_parallel_executor.cc
        ins_reduce_sole_executor.cc
        ins_v2_reduce_sole_executor.cc
)

if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
        ins_reduce_parallel_executor.cc
        ins_reduce_sole_executor.cc
        ins_v2_reduce_sole_executor.cc
)
endif()