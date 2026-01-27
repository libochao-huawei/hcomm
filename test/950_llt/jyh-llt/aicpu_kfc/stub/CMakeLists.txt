set(AICPU_KFC_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole)
set(AICPU_KFC_OPEN_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/open_source)
set(AICPU_KFC_LLT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_legacy_protobuf()
protobuf_generate(aicpu_kfc_llt aicpu_kfc_llt_c  aicpu_kfc_llt_h
                      ${CMAKE_CURRENT_SOURCE_DIR}/proto/task.proto
)

set(aicpu_kfc_llt_stub
    #/llt/hccl/aicpu_kfc/stub

    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_gdr.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_mc2.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_profiling_plugin.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_sal.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_ge.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_gert.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_error_manager.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_fe.cc

    ${aicpu_kfc_llt_c}
    ${aicpu_kfc_llt_h})

set(aicpu_kfc_list
    ${aicpu_kfc_llt_stub}

    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_gdr.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_mc2.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_profiling_plugin.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_sal.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_ge.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_gert.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_error_manager.cc
    ${AICPU_KFC_LLT_ROOT}/stub/llt_aicpu_kfc_stub_fe.cc

    ${aicpu_kfc_llt_c}
    ${aicpu_kfc_llt_h}

    #/hccl/platform/common/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/dlra_function.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/dlhns_function.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_rts.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_hal.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_hccp.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_tdt.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_error_manager.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_error_manager_pub.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_error_manager_pub_extend.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_verbs.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_qos.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/adapter/adapter_trace.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/sal.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/log.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/externalinput.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/peterson_lock.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/workflow.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/dltdt_function.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/dlhal_function.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/dlibv_function.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/dlqos_function.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/dltrace_function.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/dlprof_func.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/hccl_dl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/hccl_ip_address.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/device_capacity.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/mr_manager.cc

    #/hccl/platform/common/misc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/misc/network_manager/network_manager.cc

    #/hccl/platform/common/p2p_mgmt
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/p2p_mgmt/p2p_mgmt.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/p2p_mgmt/p2p_mgmt_common.cc

    #/hccl/platform/common/unfold_cache
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/unfold_cache/op_unfold_cache_entry.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/unfold_cache/op_unfold_cache.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/unfold_cache/op_unfold_key.cc

    #/hccl/platform/resource/mem/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/mem/mem_device.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/mem/mem_host.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/mem/heterog_mem_blocks_manager.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/mem/hdc.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/mem/mem_mapping_manager.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/mem/mem_mapping_manager_host.cc

    #/hccl/platform/resource/rma_buffer/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer/local_rdma_rma_buffer_impl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer/local_rdma_rma_buffer.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer/remote_rdma_rma_buffer_impl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer/remote_rdma_rma_buffer.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer/local_ipc_rma_buffer_impl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer/local_ipc_rma_buffer.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer/remote_ipc_rma_buffer_impl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer/remote_ipc_rma_buffer.cc

    #/hccl/platform/resource/notify/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/bare_notify.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/esched_notify.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/local_notify.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/local_notify_impl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/notify_pool.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/notify_pool_impl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/remote_notify.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/remote_notify_impl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/rts_notify.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/local_ipc_notify.cc

    #/hccl/platform/resource/netdev/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/netdev/hccl_net_dev.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/netdev/hccl_net_dev_v1.cc

    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/dispatcher_ctx/dispatcher_ctx.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/comm_primitive/data_plane_interface.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/comm_primitive/hccl_primitive_local.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/comm_primitive/hccl_primitive_remote.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/comm_primitive/hccl_dispatcher_ctx.cc

    #/hccl/platform/resource/stream
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/stream/stream.cc

    #/hccl/platform/resource/thread
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/thread/hccl_thread.cc

    #/hccl/platform/resource/socket/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/socket/hccl_socket.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/socket/hccl_network.cc

    #/hccl/platform/resource/transport/device
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/device/transport_device_p2p.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/device/transport_device_ibverbs.cc

    #/hccl/platform/resource/transport/onesided
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/transport_mem.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/transport_roce_mem.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/transport_ipc_mem.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/hccl_mem.cc

    #/hccl/platform/resource/transport/onesided/device
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/device/transport_device_roce_mem.cc

    #/hccl/platform/resource/transport/host
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/transport_ibverbs.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/transport_p2p.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/transport_net.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/transport_tcp.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/transport_roce.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/transport_shm_event.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/transport_virtural.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/transport_direct_npu.cc
    # ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/transport_muti_ibverbs.cc
    #/hccl/platform/resource/transport/heterog
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/transport_heterog.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/transport_heterog_roce.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/transport_heterog_event_tcp.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/transport_heterog_event_roce.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/transport_heterog_p2p.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/transport_heterog_raw_roce.cc
    #/hccl/platform/resource/transport
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/transport_base.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/transport.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/tcp_send_thread_pool.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/mem_name_repository.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/tcp_recv_task.cc

    #/hccl/platform/debug
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/socket.cc

    ${AICPU_KFC_CODE_ROOT}/hccl/platform/debug/traceinfo/hccl_trace_info.cc

    #/hccl/platform/task/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/dispatcher.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/dispatcher_common.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/dispatcher_graph.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/dispatcher_virtural.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/task_logic_info.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/dispatcher_aicpu.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/rtsq_interact/aicpu_hccl_sqcq.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/rtsq_interact/aicpu_hccl_sqcqv1.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/rtsq_interact/aicpu_hccl_sqcqv2.cc

    #/hccl/platform/typical
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/typical/interface_hccl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/typical/send_recv_executor.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/typical/typical_mr_manager.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/typical/typical_qp_manager.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/typical/typical_sync_mem.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/typical/typical_window_mem.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/typical/rdma_resource_manager.cc

    #/hccl/platform/hccl_tbe_task
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/common/legacy_common.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/graph_ctx_mgr/graph_ctx_mgr_common.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/graph_ctx_mgr/graph_ctx_mgr.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/graph_ctx_mgr/ffts_ctx_provider.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/tbe_common.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/tbe_vector_reduce.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/tbe_crack_cleared.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/eletwise_v1.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/eletwise_v2.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/eletwise_v3.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/eletwise_v4.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/auto_tiling_rt2.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/elewise_memset.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/tbe_gatherv2_aicore.cpp
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/tbe_unsorted_segment_sum_aicore.cpp
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/segment_dsl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/legacy_log.cc

    #/hccl/platform/pingmesh
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/ping_mesh/ping_mesh.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/ping_mesh/hccn_rping.cc

    #/hccl/platform/remote_access
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/remote_access/comm_remote_access.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/remote_access/remote_access.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/remote_access/remote_access_impl.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/remote_access/transport_remote_access.cc

    #/hccl/hccl/hccl/hccl_comm
    ${AICPU_KFC_CODE_ROOT}/hccl/hccd/hccl_comm_conn_mgr.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccd/hccl_comm_conn.cc

    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/aicpu_kfc_interface.cc

    #/hccl/framework/device/aicpu_kfc/algorithm
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/algorithm/aicpu_allgather.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/algorithm/aicpu_allreduce.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/algorithm/aicpu_dispatcher.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/algorithm/task_orchestrator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/algorithm/aicpu_dmy_cal_allreduce.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/algorithm/aicpu_reduce_scatter.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/algorithm/aicpu_alltoall.cc

    #/hccl/framework/device/common/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/common/sqe_context_utils.cc

    #/hccl/framework/device/aicpu_kfc/common/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/common/aicpu_sqe_context.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/common/aicpu_kfc_utils.cc

    #/hccl/framework/device/aicpu_kfc/framework/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/framework/aicpu_kfc_batchwrite_process.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/framework/aicpu_kfc_deprecated_process.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/framework/aicpu_kfc_process.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/framework/aicpu_kfc_prof.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/framework/aicpu_kfc_retry_process.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/framework/aicpu_kfc_rpc_server.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/framework/aicpu_kfc_rpc_serverv2.cc

    #/hccl/framework/device/aicpu_kfc/dfx/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/dfx/aicpu_executor_tracer.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/dfx/aicpu_profiling_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/dfx/mc2_trace_utils.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/aicpu_kfc/dfx/dfx_extend_info.cc

    #/hccl/framework/device/debug/dfx/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/debug/dfx/trace/executor_tracer.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/debug/dfx/trace/cann_error_reporter.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/debug/dfx/profiling/profiling_extend_info.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/debug/dfx/profiling/profiling_manager_device.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/debug/dfx/profiling/profiling_command_handle.cc

    #/hccl/framework/device/framework/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/framework/aicpu_hccl_process.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/framework/aicpu_communicator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/framework/aicpu_one_side_service.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/framework/aicpu_hdc.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/framework/aicpu_share_data_manager.cc

    #这个文件是软连接过来的
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/framework/aicpu_zero_copy_exchanger.cc

    #/hccl/framework/device/utils/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/utils/hccl_aicpu_utils.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/utils/aicpu_hdc_utils.cc

    #/hccl/framework/device/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/device/hccl_aicpu_interface.cc

    ${AICPU_KFC_CODE_ROOT}/hccl/hccd/hccd.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccd/hccl_raw.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccd/hccd_comm.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccd/hccd_impl_pml.cc


    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/hcom/hcom.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/hcom/gradient_segment/gradient_segment.cc

    #/hccl/cltm
    ${AICPU_KFC_CODE_ROOT}/cltm/src/cltm.cc
    ${AICPU_KFC_CODE_ROOT}/cltm/src/rank_table.cc

    #/hccl/hcom_graph_adaptor/ge_plugin
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/plugin_manager.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom_plugin.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/common/hcom_op_utils.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/common/op_fusion_base.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/common/ops_kernel_info_store_base.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/common/ops_kernel_builder_base.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/common/hcom_base_funs.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_all_reduce_fusion.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_alltoallvc_fusion.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_allgather_fusion.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_reducescatter_fusion.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_broadcast_fusion.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_reduce_fusion.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_ops_kernel_builder.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_fusion_optimizer.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_graph_optimizer.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_ops_kernel_info_store.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/offline_build_config_parse.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_graph_mc2.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/hcom_graph_superkernel.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/lowering/hcom_build_graph.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/lowering/hcom_launch_kernel.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/lowering/hcom_node_converter.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hvd/hvd_adapter.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hvd/hvd_all_reduce_fusion.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hvd/hvd_graph_optimizer.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hvd/hvd_ops_kernel_info_store.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hvd/hvd_ops_kernel_builder.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_hcom_all_reduce_fusion.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_hcom_graph_optimizer.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_hcom_ops_kernel_builder.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_hcom_ops_kernel_info_store.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_plugin.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool/communication.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool/cluster.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool/evaluator.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool/layers.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool/model.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool/topology.cc

    #/hccl/hcom_graph_adaptor/hcom_executor
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/hcom_executor/hcom_executor.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/hcom_executor/executor_message.cc

    #/hccl/hcom_graph_adaptor/evaluator
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/comm_time_evaluator.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/connected_graph_analyzer.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/connected_graph_builder.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/connected_graph.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/connected_subgraph_factory.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/hockney_model.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/topo_detect.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/topo_split.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/connected_subgraph/connected_subgraph.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/connected_subgraph/subgraph_mesh.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/connected_subgraph/subgraph_mutiring.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/connected_subgraph/subgraph_ring.cc
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/connected_subgraph/subgraph_star.cc

    #/hccl/hcom_gradient_split_tune
    ${AICPU_KFC_CODE_ROOT}/hcom_gradient_split_tune/src/gradient_split_tune.cc

    # hccl/hccl/algorithm/base/communicator
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/communicator/calc_nb_transport_req.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/communicator/calc_nhr_transport_req.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/communicator/calc_nhr_v1_transport_req.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/communicator/calc_ahc_transport_req_base.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/communicator/calc_ahc_transport_req.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/communicator/calc_ahc_broke_transport_req.cc

    # hccl/hccl/algorithm/base/alg_template
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/alg_template_register.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/nonuniform_hierarchical_ring_base.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather/all_gather_nhr.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_nhr.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_broadcast/broadcast_nhr.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_nhr.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_scatter/scatter_nhr.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce/reduce_nhr_oneshot.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_broadcast/broadcast_nhr_oneshot.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_nhr_oneshot.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/nonuniform_hierarchical_ring_v1_base.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather/all_gather_nhr_v1.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_nhr_v1.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_broadcast/broadcast_nhr_v1.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_nhr_v1.cc

    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_hd_stage.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_local_reduce.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_plant_local_reduce.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_plant_local_reduce_combine.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/nonuniform_bruck_base.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather/all_gather_hd_stage.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather/all_gather_nb.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather/all_gather_pipeline.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_nb.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_broadcast/broadcast_nb.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_broadcast/broadcast_nb_binary.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_scatter/scatter_nb.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_scatter/scatter_ring_direct.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_nb.cc

    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/alg_template_base.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_base.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_nslb.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_alg_template_base.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather/all_gather_ahc.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather/all_gather_ahc_broke.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_ahc.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_ahc_broke.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ahc.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ahc_broke.cc

    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_reduce_broadcast.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_mesh_opbase.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_chunk_mesh.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_local_reduce.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_local_reduce_bcast.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_mesh_oneshot.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_doubling.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_doubling_direct.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_doubling_local_reduce.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_pipeline.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_opbase_pipeline.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoall/allltoall_pipeline_base.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoall/allltoall_pipeline_mesh_pairwise_ping_pong.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoall/allltoall_pipeline_mesh_pairwise_ccl_enough.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoallv/alltoallv_direct_fullmesh.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoallv/alltoallv_for_310p.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce/all_reduce_hd_optim.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_broadcast/broadcast_oneshot.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather/all_gather_unified_march.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_unified_march.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/component/reducer.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/component/sender.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/mc2_handler/mc2_handler.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/alg_env_config.cc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/device/transport_device_roce_mem.cc
)

set(aicpu_kfc_list_open
    #framework/cluster_maintenance/health/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/health/heartbeat/heartbeat.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/detect/detect_connect_anomalies/detect_connect_anomalies.cc


    #framework/cluster_maintenance/recovery
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry/opretry_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry/opretry_agent.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry/opretry_server.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry/opretry_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry/opretry_link_manage.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry/opretry_connection.cc

    #/hccl/platform/common/src
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/param_check.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/config.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/json_utils.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/read_write_lock_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/read_write_lock.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/config/env_config.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/config/env_config_host.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/hccl_hash_utils.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/task/callback_thread_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/task/hccl_callback_task.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/opexecounter/opexecounter.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/order_launch/order_launch.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_ranktableParser.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_ranktableStandard.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_ranktableConcise.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_ranktableHeterog.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_roletableParser.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_ranktableOffline.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_ranktable_partition.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_exchange_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_exchange_agent.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_exchange_dispatcher.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_exchange_server.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_detect.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/topoinfo_parse.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo/hccl_whitelist.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/exception/exception_handler.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/thread/threads_guard.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/launch_aicpu.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/onesided_memory_management/global_mem_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/onesided_memory_management/global_mem_record.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/hccl_communicator_attrs.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/hccl_communicator_attrs_host.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/hccl_comm.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/hccl_comm_host.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/comm_config.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/comm_configer.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/hccl_communicator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/hccl_communicator_host.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/aclgraph/zero_copy_acl_graph.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/task_abort_handler.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/resource_manager/preempt_port_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/resource_manager/transport_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/resource_manager/multi_qpInfo_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/one_sided_service/i_hccl_one_sided_service.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/one_sided_service/hccl_one_sided_service.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/one_sided_service/hccl_one_sided_conn.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/one_sided_service/one_sided_service_adapt.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/zero_copy/zero_copy_memory_agent.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/zero_copy/zero_copy_address_mgr.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/zero_copy/zero_copy_address_mgr_host.cc

    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/hcom/hcom_common.cc

    #/hccl/framework/op_base/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/op_base/src/op_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/op_base/src/op_base_mc2.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/op_base/src/op_base_host.cc

    #/hccl/framework/nslbdp
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/nslbdp/hccl_nslbdp.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/nslbdp/hccl_nslb_md5.cc

    # hccl/hccl/algorithm/base/communicator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/topo_info_extractor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy/comm_factory.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy/comm_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy/comm_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy/comm_star.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy/comm_p2p.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy/comm_mesh.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy/comm_halving_doubling.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/calc_hd_transport_req.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/calc_mesh_transport_req.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/calc_p2p_transport_req.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/calc_partial_mesh_transport_req.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/calc_ring_transport_req.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/calc_transport_req_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/calc_hccs_plus_sio_transport_req.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/alg_env_config.cc

    # hccl/hccl/algorithm/base/alg_template
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/aligned_reduce_scatter_double_ring_with_serial_local_copy.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/aligned_reduce_scatter_double_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/all_gather_halving_doubling.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/all_gather_mesh.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/all_gather_mesh_direct.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/all_gather_mesh_atomic.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/all_gather_mesh_mix.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/aligned_all_gather_double_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/all_gather_ring_concurrent_direct.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/all_gather_ring_direct.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/all_gather_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_reduce/all_reduce_recursive_hd.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_reduce/all_reduce_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_broadcast/bcast_recursive_halvingdoubling.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_broadcast/broadcast_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_broadcast/broadcast_star.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_gather/gather_star.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/recursive_halvingdoubling_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce/reduce_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_halving_doubling.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_recursive_hd.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ring_concurrent_direct.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_atomic.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_atomic_opbase.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_mix.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_mix_single_stream.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_scatter/scatter_mesh.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_scatter/scatter_ring_concurrent_direct.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_scatter/scatter_double_ring_direct.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_scatter/scatter_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_scatter/multi_root_scatter_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_send_recv/send_receive.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather/all_gather_recursive_hd.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_gather/gather_ring.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_gather/gather_mesh.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce/reduce_recursive_hd.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_alltoallv/alltoallv_pairwise.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_pairwise.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_mesh.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_calculator.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_all_gather_v
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_mesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_mesh_graph_executor.cc

    # hccl/hccl/algorithm/impl
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_alg_utils.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/alg_configurator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/hccl_alg.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/hccl_aiv.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/hccl_aiv_host.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/topo_matcher.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/task/parallel_task_loader.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/task/task_loader.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/task/threadManage.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/coll_alg_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/scatter_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/reduce_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/all_reduce_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/reduce_scatter_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/reduce_scatter_v_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/all_gather_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/all_gather_v_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/broadcast_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/send_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/receive_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/batchsendrecv_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/alltoall_operator.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/registry/coll_alg_op_registry.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/legacy/hccl_impl.cc

    # hccl/hccl/common/health
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/health/rank_consistentcy_checker.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/health/calc_crc.cc

    # hccl/hccl/common/debug
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/task_profiling.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/task_exception_handler.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/task_exception_handler_host.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/task_overflow.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/plugin_runner.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/plugin_runner_host.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/profiler_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/command_handle.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/profiling_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/profiling_manager_pub.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/profiler_manager_impl.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/profiler_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/adapter_prof.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/dlprof_function.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/dlrt_function.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/config/config_log.cc

    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/launch_device/launch_device.cc

    # hccl/hccl/common/stream
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/stream/stream_utils.cc

    # hccl/hccl/algorithm/impl/resource_manager
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager/queue_notify_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager/workspace_mem.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager/workspace_resource.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager/workspace_resource_impl.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager/ccl_buffer_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager/offload_stream_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager/op_base_stream_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager/hccl_socket_manager.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager/stream_active_manager.cc

    # hccl/hccl/algorithm/impl/coll_executor
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_executor_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_native_executor_base.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/alg_profiling.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_comm_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/registry/coll_alg_exec_registry.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_all_gather
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_comm_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_aligned_all_gather_double_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_opbase_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_opbase_pipeline_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_single_rank_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_small_count_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mix_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_aiv_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_aiv_smallcount_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_semi_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_aiv_rdma_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_aiv_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/310P/coll_all_gather_for_310p_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_all_gather_v
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/310P/coll_all_gatherv_for_310p_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_mesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gatherv_mesh_aiv_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gatherv_mesh_aiv_smallcount_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_all_reduce
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_single_rank_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_small_count_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_comm_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_fast_double_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_aligned_all_reduce_double_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_mid_count_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_oneshot_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_opbase_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_opbase_pipeline_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_reduce_plus_bcast_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_aiv_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_small_count_aiv_rdma_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mid_count_aiv_rdma_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_aiv_smallcount_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mix_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_order_preserved_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/310P/coll_all_reduce_for_310p_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/310P/coll_all_reduce_for_310p_doubling_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/310P/coll_all_reduce_for_310p_doubling_direct_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_aiv_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_smallcount_for_910_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_reduce
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce/coll_reduce_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce/coll_reduce_mesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce/coll_reduce_ring_plus_hd_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce/coll_reduce_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce/coll_reduce_comm_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce/coll_reduce_single_rank_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_send_receive
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_send_receive/coll_batch_send_recv_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_send_receive/coll_batch_send_recv_retry_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_send_receive/coll_send_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_send_receive/coll_receive_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_reduce_scatter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_comm_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_deter_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_fast_double_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_aligned_reduce_scatter_double_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_dma_elimination.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_opbase_pipeline_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_single_rank_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mix_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_aiv_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_aiv_smallcount_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_semi_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_aiv_rdma_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_order_preserved_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/310P/coll_reduce_scatter_for_310p_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor_host.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_single_rank_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_2level_pipeline_excecutor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_fullmesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_staged_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_mesh_aiv_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_direct_fullmesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_staged_aiv_rdma_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter/coll_scatter_comm_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter/coll_scatter_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter/coll_scatter_mesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter/coll_scatter_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter/coll_scatter_single_rank_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter/coll_scatter_ring_for_910_93_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P/coll_reduce_scatter_v_for_310p_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_mesh_opbase_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_aiv_big_count_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_mesh_aiv_smallcount_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_mesh_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_broadcast
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_ring_for_910_93_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_mesh_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_ring_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_comm_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_mix_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/310P/coll_broadcast_for_310p_comm_executor.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/310P/coll_broadcast_plus_broadcast.cc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_smallcount_executor.cc
)

add_library(aicpu_kfc_llt SHARED ${aicpu_kfc_llt_stub} ${aicpu_kfc_list})
#头文件搜索路径
target_include_directories(aicpu_kfc_llt  PRIVATE
    # # inc目录下头文件
    ${TOP_DIR}/inc
    ${TOP_DIR}/metadef/
    ${TOP_DIR}/metadef/inc
    ${TOP_DIR}/metadef/pkg_inc
    ${TOP_DIR}/metadef/inc/common/util/trace_manager
    ${TOP_DIR}/metadef/inc/common/util/error_manager
    ${TOP_DIR}/metadef/inc/register
    ${TOP_DIR}/metadef/inc/graph
    ${TOP_DIR}/inc/external
    ${TOP_DIR}/metadef/inc/external
    ${TOP_DIR}/ace/npuruntime/runtime/platform/inc
    ${TOP_DIR}/metadef/inc/external/graph
    ${TOP_DIR}/metadef/inc/common
    ${TOP_DIR}/inc/aicpu/
    ${TOP_DIR}/inc/aicpu/aicpu_schedule/aicpu_sharder
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/metadef/
    ${TOP_DIR}/air/inc/
    ${TOP_DIR}/air/inc/external
    ${TOP_DIR}/abl/atrace/inc/utrace
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/abl/qos/qosmng
    ${TOP_DIR}/ace/comop/inc
    ${TOP_DIR}/ace/comop/inc/hccl
    ${TOP_DIR}/asl/hss/inc
    ${TOP_DIR}/asl/hss/inc/pub
    ${TOP_DIR}/asl/hss/common/hash_table
    ${TOP_DIR}/inc/aicpu/common
    ${TOP_DIR}/ace/csruntime/inc/tsch
    ${TOP_DIR}/asc/asc-devkit
    ${TOP_DIR}/asc/asc-devkit/impl
    ${TOP_DIR}/asc/asc-devkit/include

    ${AICPU_KFC_LLT_ROOT}/stub
    ${TOP_DIR}/hlt/hccl/third_party/mellanox/4.2/include
    ${TOP_DIR}/open_source/json/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/mpi/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include/3rdparty

    # hccl/hccl目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hccl/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/pub_inc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/impl
    ${AICPU_KFC_OPEN_CODE_ROOT}/inc/hccl
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/inc/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/inc/host/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/health/heartbeat/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/host
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/config
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/task
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/exception
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/thread
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/onesided_memory_management
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/one_sided_service
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/resource_manager
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/hcom/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/op_base/src/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/nslbdp/

    ${AICPU_KFC_CODE_ROOT}/hccl/platform/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/unique
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/buffer_manager
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/unfold_cache
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc/adapter
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc/adapter/host
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc/common/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/device/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/device
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/socket
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/mem
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/aiv_communication/
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/comm/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/executor/component/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/impl
    ${AICPU_KFC_CODE_ROOT}/hccl/hdcs
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/op_base/src
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl_heterog/common
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl_heterog/heterog_cpu
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl_heterog/rpc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccd
    ${AICPU_KFC_CODE_ROOT}/hccl/hcom/src/
    ${AICPU_KFC_CODE_ROOT}/hccl/hcom/src/gradient_segment
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/device/
    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc/
    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc/inner
    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc/aicpu
    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc/new
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/dispatcher_ctx

    # hccl/common 目录下头文件
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/health/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/config
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/inc/host
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/stream/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/launch_device/

    # hccl/algorithm 目录下头文件
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_aiv_template
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/inc_all_reduce_deter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/mc2_handler
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/legacy
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/legacy/operator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/task
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/registry
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/registry
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_send_receive
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter
    ${AICPU_KFC_OPEN_CODE_ROOT}/pkg_inc

    # hccl/hcom_graph_adaptor目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/hcom_executor
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/common
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hvd
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/auto_tuning
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/lowering
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/

    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoall
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoallv
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_broadcast
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_scatter
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/component
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/pub_inc

    # hccl/hcom_gradient_split_tune目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hcom_gradient_split_tune/inc

    # hccl/cltm目录下头文件
    ${AICPU_KFC_CODE_ROOT}/cltm/src

    # hccl/operator目录下头文件
    ${AICPU_KFC_CODE_ROOT}/operator

    # /hccl/framework/device/目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/common
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/aicpu_kfc
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/aicpu_kfc/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/aicpu_kfc/common

    # proto.h头文件
    ${CMAKE_BINARY_DIR}/proto/aicpu_kfc_llt

    # dump头文件
    ${TOP_DIR}/abl/adump/inc/adump
    # tbe头文件
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/common/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/graph_ctx_mgr/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/inc/
)

add_library(aicpu_kfc_llt_open SHARED ${aicpu_kfc_list_open})
#头文件搜索路径
target_include_directories(aicpu_kfc_llt_open  PRIVATE
    # # inc目录下头文件
    ${TOP_DIR}/inc
    ${TOP_DIR}/metadef/
    ${TOP_DIR}/metadef/inc
    ${TOP_DIR}/metadef/pkg_inc
    ${TOP_DIR}/metadef/inc/common/util/trace_manager
    ${TOP_DIR}/metadef/inc/common/util/error_manager
    ${TOP_DIR}/metadef/inc/register
    ${TOP_DIR}/metadef/inc/graph
    ${TOP_DIR}/inc/external
    ${TOP_DIR}/metadef/inc/external
    ${TOP_DIR}/ace/npuruntime/runtime/platform/inc
    ${TOP_DIR}/metadef/inc/external/graph
    ${TOP_DIR}/metadef/inc/common
    ${TOP_DIR}/inc/aicpu/
    ${TOP_DIR}/inc/aicpu/aicpu_schedule/aicpu_sharder
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/metadef/
    ${TOP_DIR}/air/inc/
    ${TOP_DIR}/air/inc/external
    ${TOP_DIR}/abl/atrace/inc/utrace
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/abl/qos/qosmng
    ${TOP_DIR}/ace/comop/inc
    ${TOP_DIR}/ace/comop/inc/hccl
    ${TOP_DIR}/asl/hss/inc
    ${TOP_DIR}/asl/hss/inc/pub
    ${TOP_DIR}/asl/hss/common/hash_table
    ${TOP_DIR}/inc/aicpu/common
    ${TOP_DIR}/ace/csruntime/inc/tsch
    ${TOP_DIR}/asc/asc-devkit
    ${TOP_DIR}/asc/asc-devkit/impl
    ${TOP_DIR}/asc/asc-devkit/include

    ${AICPU_KFC_LLT_ROOT}/stub
    ${TOP_DIR}/hlt/hccl/third_party/mellanox/4.2/include
    ${TOP_DIR}/open_source/json/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/mpi/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include/3rdparty

    # hccl/hccl目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hccl/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/pub_inc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/impl
    ${AICPU_KFC_OPEN_CODE_ROOT}/inc/hccl
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/inc/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/inc/host
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/health/heartbeat/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/host
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/task
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/config
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/exception
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/thread
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/onesided_memory_management
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/one_sided_service
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/resource_manager
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/hcom/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/op_base/src/

    ${AICPU_KFC_CODE_ROOT}/hccl/platform/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/unique
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/buffer_manager
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/unfold_cache
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc/adapter
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc/adapter/host
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc/common/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/device/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/device
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/socket
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/mem
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/aiv_communication/
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/comm/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/executor/component/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/impl
    ${AICPU_KFC_CODE_ROOT}/hccl/hdcs
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/op_base/src
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl_heterog/common
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl_heterog/heterog_cpu
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl_heterog/rpc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccd
    ${AICPU_KFC_CODE_ROOT}/hccl/hcom/src/
    ${AICPU_KFC_CODE_ROOT}/hccl/hcom/src/gradient_segment

    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc/
    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc/inner

    # hccl/common 目录下头文件
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/health/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/inc/host
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/config
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/stream/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/launch_device/

    # hccl/algorithm 目录下头文件
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_aiv_template
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/inc_all_reduce_deter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/mc2_handler
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/legacy
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/legacy/operator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/task
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/registry
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/registry
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_send_receive
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter

    # hccl/hcom_graph_adaptor目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/hcom_executor
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/common
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hvd
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/auto_tuning
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/lowering
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator/

    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoall
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoallv
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_broadcast
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_scatter
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/inc_all_reduce_deter
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template/component
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/base/alg_template
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/pub_inc

    # hccl/hcom_gradient_split_tune目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hcom_gradient_split_tune/inc

    # hccl/cltm目录下头文件
    ${AICPU_KFC_CODE_ROOT}/cltm/src

    # hccl/operator目录下头文件
    ${AICPU_KFC_CODE_ROOT}/operator

    # /hccl/framework/device/目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/common
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/aicpu_kfc
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/aicpu_kfc/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/aicpu_kfc/common
 
    # proto.h头文件
    ${CMAKE_BINARY_DIR}/proto/aicpu_kfc_llt
    # dump头文件
    ${TOP_DIR}/abl/adump/inc/adump
    # tbe头文件
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/common/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/graph_ctx_mgr/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/inc/
)

target_compile_definitions(aicpu_kfc_llt    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(aicpu_kfc_llt   PRIVATE
     -U_FORTIFY_SOURCE -g
)

target_link_libraries(aicpu_kfc_llt  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:msprof_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:runtime_headers>
    $<BUILD_INTERFACE:cce_headers>
    $<BUILD_INTERFACE:network_headers>
    $<BUILD_INTERFACE:kernel_tiling_headers>
    c_sec_static
    ascend_protobuf
    mmpa
    -lrt
    -ldl
)

target_compile_definitions(aicpu_kfc_llt_open    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(aicpu_kfc_llt_open   PRIVATE
     -U_FORTIFY_SOURCE -g
)
    
target_link_libraries(aicpu_kfc_llt_open  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:msprof_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:runtime_headers>
    $<BUILD_INTERFACE:cce_headers>
    $<BUILD_INTERFACE:network_headers>
    $<BUILD_INTERFACE:kernel_tiling_headers>
    aicpu_kfc_llt
    c_sec_static
    ascend_protobuf
    mmpa
    -lrt
    -ldl
)
