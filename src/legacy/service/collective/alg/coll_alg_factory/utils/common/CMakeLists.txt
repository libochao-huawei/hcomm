if((NOT BUILD_OPEN_PROJECT) OR DEVICE_MODE)
target_sources(ccl_kernel PRIVATE
)
target_sources(hccl_v2_native_alg_repo PRIVATE
)
endif()