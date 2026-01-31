target_sources(hccl_v2_native_alg_repo PRIVATE
        template_utils.cc
)

if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
        template_utils.cc
)
endif()

add_subdirectory(ccu_alg_template)
add_subdirectory(prim_alg_template)
add_subdirectory(aiv_alg_template)
add_subdirectory(ins_alg_template)

