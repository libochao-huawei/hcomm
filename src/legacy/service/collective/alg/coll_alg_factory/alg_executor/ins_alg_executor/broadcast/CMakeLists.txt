target_sources(hccl_v2_native_alg_repo PRIVATE
        ins_broadcast_parallel_executor.cc
        ins_broadcast_parallel_aicpu_executor.cc
        ins_broadcast_sole_executor.cc
        ins_v2_broadcast_sole_executor.cc
)

if((NOT BUILD_OPEN_PROJECT) OR DEVICE_MODE)
target_sources(ccl_kernel PRIVATE
        ins_broadcast_parallel_executor.cc
        ins_broadcast_parallel_aicpu_executor.cc
        ins_broadcast_sole_executor.cc
        ins_v2_broadcast_sole_executor.cc
)
endif()