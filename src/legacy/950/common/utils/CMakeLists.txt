target_sources(hccl_v2 PRIVATE
        hash_utils.cc)

if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
        hash_utils.cc
)
endif()