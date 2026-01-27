set(hccl_utest_impl_list
    main.cc
    ut_external_input.cc
    ut_hccl_impl.cc
    ut_hccl_impl_alg.cc
    #ut_hccl_impl_alg_nhr.cc
    #ut_hccl_impl_alg_nhr_v1.cc
    #ut_hccl_impl_alg_nb.cc
    #ut_hccl_impl_alg_ahc.cc
    #ut_hccl_impl_alg_double_ring.cc
    ut_gradient_split_tune.cc
    ut_exchanger_network.cc
    ut_dlqosfunction.cc
    ut_dispatcher_common.cc
    ut_cltm.cc
    ut_checkcrc.cc
    ut_callback_thread_manager.cc
    ut_auto_tuning_hcom_all_reduce_fusion.cc
    #ut_alltoallv_staged.cc
    #ut_allreduce_mesh_opbase.cc
    ut_allreduce_aiv.cc
    ut_allreduce_aiv_rdma.cc
    ut_alltoall_aiv.cc
    #ut_all_reduce_mesh_oneshot.cc
    #ut_all_reduce_chunk_mesh.cc
    ut_adapter.cc
    ut_template_base.cc
    ut_netdevice.cc
    ut_global_mem_mgr.cc
    ut_multi_qp_info.cc
)
add_executable(hccl_utest_impl  ${hccl_utest_impl_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_impl  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_impl    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_impl   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1 -g
)

target_link_libraries(hccl_utest_impl  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:msprof_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:runtime_headers>
    $<BUILD_INTERFACE:cce_headers>
    $<BUILD_INTERFACE:network_headers>
    $<BUILD_INTERFACE:atrace_headers>
    c_sec_static
    ascend_protobuf
    hccl_llt
    hccl_llt_open
    -lrt
    -ldl
#    exe_graph
)

add_dependencies(hccl_utest_impl hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_impl
   TASK_NUM 1
)
