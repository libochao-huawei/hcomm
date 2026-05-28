if(TARGET hccl_v2)
    target_sources(hccl_v2 PRIVATE
            ins_reduce_parallel_executor.cc
            ins_reduce_sole_executor.cc
            ins_v2_reduce_sole_executor.cc
    )
endif()

if(TARGET ccl_kernel)
    target_sources(ccl_kernel PRIVATE
            ins_reduce_parallel_executor.cc
            ins_reduce_sole_executor.cc
            ins_v2_reduce_sole_executor.cc
    )
endif()