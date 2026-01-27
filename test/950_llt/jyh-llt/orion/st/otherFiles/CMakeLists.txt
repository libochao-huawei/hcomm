add_subdirectory(rank_table_info)
add_subdirectory(common)
add_subdirectory(phy_topo)
add_subdirectory(rank_graph)
add_subdirectory(phy_topo_builder)
add_subdirectory(framework)
add_subdirectory(unified_platform)
add_subdirectory(stream)
add_subdirectory(topo_info)
add_subdirectory(device_mode)
add_subdirectory(rank_info_detect)
add_subdirectory(rank_graph_64_plus_1)

target_sources(hccl_v2_stest
        PUBLIC
        st_coverResult.cc
        whitelist_test.cc
        ##st_ins_rules.cc
        st_instructions.cc
        st_connections_builder.cc
        st_prim_rules.cc
        st_hccl_sqe_v82.cc
        st_hccl_sqe.cc
        st_sqe_mgr.cc

        st_lite_res.cc
        st_alg_topo_package_helper.cc
        st_binary_stream.cc
        st_ins_executor.cc

        st_kernel_entrance.cc
        st_communicator_impl_lite.cc

        st_coll_service_default_impl.cc
        st_alg_component_stub.cc

        st_mask_event.cc
        st_coll_service_ai_cpu_impl.cc
        st_interpreter.cc
)
