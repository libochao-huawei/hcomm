target_sources(hccl_v2 PRIVATE
        ins_broadcast_parallel_executor.cc
        ins_broadcast_parallel_aicpu_executor.cc
        ins_broadcast_sole_executor.cc
        ins_v2_broadcast_sole_executor.cc
)

if(TARGET ccl_kernel)
target_sources(ccl_kernel PRIVATE
        ins_broadcast_parallel_executor.cc
        ins_broadcast_parallel_aicpu_executor.cc
        ins_broadcast_sole_executor.cc
        ins_v2_broadcast_sole_executor.cc
)
endif()
