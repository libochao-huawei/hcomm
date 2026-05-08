if(TARGET ccl_kernel)
    target_sources(ccl_kernel PRIVATE
    )
endif()

if(TARGET ccl_kernel)
if(TARGET hccl_v2)
    target_sources(hccl_v2 PRIVATE
    )
endif()
endif()