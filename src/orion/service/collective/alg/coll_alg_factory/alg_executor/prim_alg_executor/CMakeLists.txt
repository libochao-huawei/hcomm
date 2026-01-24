target_sources(hccl_v2_native_alg_repo PRIVATE

        coll_alg_base.cc
        all_gather_sole_executor.cc
        all_gather_seq_executor.cc

        reduce_scatter_sole_executor.cc
        all_reduce_comb_executor.cc

)

if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
        all_gather_seq_executor.cc
        all_gather_sole_executor.cc
        all_reduce_comb_executor.cc
        coll_alg_base.cc
        reduce_scatter_sole_executor.cc
)
endif()