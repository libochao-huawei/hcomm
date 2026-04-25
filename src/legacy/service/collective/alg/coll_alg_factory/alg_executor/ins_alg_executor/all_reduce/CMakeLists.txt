target_sources(hccl_v2 PRIVATE
        ins_all_reduce_parallel_executor.cc
        ins_all_reduce_sole_executor.cc
        ins_v2_all_reduce_sole_executor.cc
        ins_all_reduce_parallel_executor_opt.cc
)

if(TARGET ccl_kernel)
target_sources(ccl_kernel PRIVATE
        ins_all_reduce_parallel_executor.cc
        ins_all_reduce_sole_executor.cc
        ins_v2_all_reduce_sole_executor.cc
        ins_all_reduce_parallel_executor_opt.cc
)
endif()