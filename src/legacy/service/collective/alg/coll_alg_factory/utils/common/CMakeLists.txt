if(NOT BUILD_OPEN_PROJECT OR DEVICE_MODE)
target_sources(ccl_kernel PRIVATE
)
target_sources(hccl_v2 PRIVATE
)
endif()