target_sources(hccl_v2 PRIVATE
        template_utils.cc
)

if(TARGET ccl_kernel)
target_sources(ccl_kernel PRIVATE
        template_utils.cc
)
endif()

add_subdirectory(ccu_alg_template)
add_subdirectory(prim_alg_template)
add_subdirectory(aiv_alg_template)
add_subdirectory(ins_alg_template)

