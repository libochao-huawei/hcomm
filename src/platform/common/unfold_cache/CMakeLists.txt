set(src_list
    op_unfold_cache_entry.cc
    op_unfold_cache.cc
    op_unfold_key.cc
)

if(TARGET hccl_plf)
    target_sources(hccl_plf PRIVATE
        ${src_list}
    )
endif()

if(TARGET hccd)
    target_sources(hccd PRIVATE
        ${src_list}
    )
endif()

if(TARGET ccl_kernel_plf)
    target_sources(ccl_kernel_plf PRIVATE
        ${src_list}
    )
endif()

if(TARGET ccl_kernel_plf_a)
    target_sources(ccl_kernel_plf_a  PRIVATE
        ${src_list}
    )
endif()