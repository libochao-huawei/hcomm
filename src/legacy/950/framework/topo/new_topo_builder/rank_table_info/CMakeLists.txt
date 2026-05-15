target_sources(hccl_v2 PRIVATE
    rank_table_info.cc
    rank_level_info.cc
    new_rank_info.cc
    changed_rank_info.cc
    control_plane.cc
    address_info.cc
)

if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
    rank_level_info.cc
    rank_table_info.cc
    new_rank_info.cc
    control_plane.cc
    address_info.cc
)
endif()