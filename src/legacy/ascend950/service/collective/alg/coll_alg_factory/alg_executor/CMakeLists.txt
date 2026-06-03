if(TARGET hccl_v2)
    target_sources(hccl_v2 PRIVATE
    
            executor_utils.cc
    
    )
endif()
if(TARGET ccl_kernel)
    target_sources(ccl_kernel PRIVATE
            executor_utils.cc
    )
endif()

add_subdirectory(prim_alg_executor)
add_subdirectory(ins_alg_executor)
