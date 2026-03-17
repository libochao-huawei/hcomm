target_sources(hccl_v2_native_alg_repo PRIVATE
        ins_all_to_all_sole_executor.cc
        ins_v2_all_to_all_sole_executor.cc
        ins_v2_all_to_all_v_sole_executor.cc
)

if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
        ins_all_to_all_sole_executor.cc
        ins_v2_all_to_all_sole_executor.cc
)
endif()