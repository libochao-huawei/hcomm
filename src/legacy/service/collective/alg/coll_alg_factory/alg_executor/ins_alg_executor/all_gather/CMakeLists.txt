if(TARGET hccl_v2)
    target_sources(hccl_v2 PRIVATE
            ins_all_gather_parallel_executor.cc
            ins_all_gather_sole_executor.cc
            ins_v2_all_gather_sole_executor.cc
            ins_v2_all_gather_v_sole_executor.cc
    )
endif()

if(TARGET ccl_kernel)
    target_sources(ccl_kernel PRIVATE
            ins_all_gather_parallel_executor.cc
            ins_all_gather_sole_executor.cc
            ins_v2_all_gather_sole_executor.cc
            ins_v2_all_gather_v_sole_executor.cc
    )
endif()