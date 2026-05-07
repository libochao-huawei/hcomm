add_subdirectory(new_topo_builder)
add_subdirectory(rank_info_detect)
add_subdirectory(topo_addr_info)

target_sources(hccl_v2 PRIVATE
        virtual_topo.cc
)

if(NOT BUILD_OPEN_PROJECT OR DEVICE_MODE)
target_sources(ccl_kernel PRIVATE
        virtual_topo.cc
)
endif()
