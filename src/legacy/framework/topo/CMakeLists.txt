add_subdirectory(new_topo_builder)
add_subdirectory(rank_info_detect)
add_subdirectory(topo_addr_info)

if(TARGET hccl_v2)
    target_sources(hccl_v2 PRIVATE
            virtual_topo.cc
    )
endif()

if(TARGET ccl_kernel)
    target_sources(ccl_kernel PRIVATE
            virtual_topo.cc
    )
endif()
