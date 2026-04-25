target_sources(hccl_v2 PRIVATE
        hash_utils.cc)

if(TARGET ccl_kernel)
target_sources(ccl_kernel PRIVATE
        hash_utils.cc
)
endif()
