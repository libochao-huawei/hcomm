if(TARGET ccl_kernel)
    target_sources(ccl_kernel PRIVATE
    )
endif()

if(NOT BUILD_OPEN_PROJECT OR DEVICE_MODE)
if(TARGET hccl_v2)
    target_sources(hccl_v2 PRIVATE
    )
endif()
endif()