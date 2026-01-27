add_subdirectory(new_topo_builder)
add_subdirectory(rank_info_detect)
add_subdirectory(ranktable)

target_sources(hccl_v2_utest
        PUBLIC
        ut_link_data.cc
        ${HCCL_LLT_ROOT}/common/virtual_topo_stub/virtual_topo_stub.cc
        ${HCCL_LLT_ROOT}/common/common_interface.cc
)