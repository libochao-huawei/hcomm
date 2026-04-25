target_sources(hccl_v2 PRIVATE
        ins_scatter_parallel_executor.cc
        ins_scatter_sole_executor.cc
        ins_v2_scatter_sole_executor.cc
)

if(TARGET ccl_kernel)
target_sources(ccl_kernel PRIVATE
        ins_scatter_parallel_executor.cc
        ins_scatter_sole_executor.cc
        ins_v2_scatter_sole_executor.cc
)
endif()