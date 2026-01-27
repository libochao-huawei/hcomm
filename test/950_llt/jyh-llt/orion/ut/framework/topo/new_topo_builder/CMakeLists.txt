add_subdirectory(phy_topo)
add_subdirectory(phy_topo_builder)
add_subdirectory(rank_graph)
add_subdirectory(rank_graph_builder)
add_subdirectory(rank_table_info)
add_subdirectory(topo_info)
add_subdirectory(rank_graph_64_plus_1)

target_sources(hccl_v2_utest
        PUBLIC
)