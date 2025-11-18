# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(HCCL_CODE_ROOT ${TOP_DIR}/hcomm)
set(HCCL_LLT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_legacy_protobuf()
protobuf_generate(hccl_llt hccl_llt_c  hccl_llt_h
                      ${CMAKE_CURRENT_SOURCE_DIR}/proto/task.proto
)

set(hccl_llt_stub
    #/llt/hccl/stub
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_profiling_plugin.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_gdr.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_sal.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_fe.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_error_manager.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_mc2.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_GenRankTable.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_gert.cc

    ${hccl_llt_c}
    ${hccl_llt_h})

set(hccl_list
    ${hccl_llt_stub}
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_profiling_plugin.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_gdr.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_sal.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_fe.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_error_manager.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_mc2.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_GenRankTable.cc
    ${HCCL_LLT_ROOT}/stub/llt_hccl_stub_gert.cc
    ${hccl_llt_c}
    ${hccl_llt_h}

    #/hccl/platform/common/
    ${HCCL_CODE_ROOT}/src/platform/common/dlra_function.cc
    ${HCCL_CODE_ROOT}/src/platform/common/dlhns_function.cc
    ${HCCL_CODE_ROOT}/src/platform/common/adapter/adapter_rts.cc
    ${HCCL_CODE_ROOT}/src/platform/common/adapter/adapter_hal.cc
    ${HCCL_CODE_ROOT}/src/platform/common/adapter/adapter_hccp.cc
    ${HCCL_CODE_ROOT}/src/platform/common/adapter/adapter_tdt.cc
    ${HCCL_CODE_ROOT}/src/platform/common/adapter/adapter_error_manager.cc
    ${HCCL_CODE_ROOT}/src/platform/common/adapter/adapter_error_manager_pub.cc
    ${HCCL_CODE_ROOT}/src/platform/common/adapter/adapter_error_manager_pub_extend.cc
    ${HCCL_CODE_ROOT}/src/platform/common/adapter/adapter_verbs.cc
    ${HCCL_CODE_ROOT}/src/platform/common/adapter/adapter_trace.cc
    ${HCCL_CODE_ROOT}/src/platform/common/sal.cc
    ${HCCL_CODE_ROOT}/src/platform/common/log.cc
    ${HCCL_CODE_ROOT}/src/platform/common/externalinput.cc
    ${HCCL_CODE_ROOT}/src/platform/common/peterson_lock.cc
    ${HCCL_CODE_ROOT}/src/platform/common/workflow.cc
    ${HCCL_CODE_ROOT}/src/platform/common/dltdt_function.cc
    ${HCCL_CODE_ROOT}/src/platform/common/dlhal_function.cc
    ${HCCL_CODE_ROOT}/src/platform/common/dlibv_function.cc
    ${HCCL_CODE_ROOT}/src/platform/common/dltrace_function.cc
    ${HCCL_CODE_ROOT}/src/platform/common/dlprof_func.cc
    ${HCCL_CODE_ROOT}/src/platform/common/hccl_dl.cc
    ${HCCL_CODE_ROOT}/src/platform/common/hccl_ip_address.cc
    ${HCCL_CODE_ROOT}/src/platform/common/device_capacity.cc
    ${HCCL_CODE_ROOT}/src/platform/common/mr_manager.cc

    #/src/platform/common/misc
    ${HCCL_CODE_ROOT}/src/platform/common/misc/network_manager/network_manager.cc

    #/src/platform/common/p2p_mgmt
    ${HCCL_CODE_ROOT}/src/platform/common/p2p_mgmt/p2p_mgmt.cc
    ${HCCL_CODE_ROOT}/src/platform/common/p2p_mgmt/p2p_mgmt_common.cc

    #/src/platform/resource/mem/
    ${HCCL_CODE_ROOT}/src/platform/resource/mem/mem_device.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/mem/mem_host.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/mem/heterog_mem_blocks_manager.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/mem/hdc.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/mem/mem_mapping_manager.cc

    #/src/platform/resource/rma_buffer/
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer/local_rdma_rma_buffer_impl.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer/local_rdma_rma_buffer.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer/remote_rdma_rma_buffer_impl.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer/remote_rdma_rma_buffer.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer/local_ipc_rma_buffer_impl.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer/local_ipc_rma_buffer.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer/remote_ipc_rma_buffer_impl.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer/remote_ipc_rma_buffer.cc

    #/src/platform/resource/notify/
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/bare_notify.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/esched_notify.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/local_notify.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/local_notify_impl.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/notify_pool.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/notify_pool_impl.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/remote_notify.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/remote_notify_impl.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/rts_notify.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/local_ipc_notify.cc

    #/src/platform/resource/netdev/
    ${HCCL_CODE_ROOT}/src/platform/resource/netdev/hccl_net_dev.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/netdev/hccl_net_dev_v1.cc

    ${HCCL_CODE_ROOT}/src/platform/resource/dispatcher_ctx/dispatcher_ctx.cc
    ${HCCL_CODE_ROOT}/src/platform/comm_primitive/hccl_primitive_local.cc
    ${HCCL_CODE_ROOT}/src/platform/comm_primitive/hccl_primitive_remote.cc
    ${HCCL_CODE_ROOT}/src/platform/comm_primitive/hccl_dispatcher_ctx.cc

    #/src/platform/resource/stream
    ${HCCL_CODE_ROOT}/src/platform/resource/stream/stream.cc

    #/src/platform/resource/socket/
    ${HCCL_CODE_ROOT}/src/platform/resource/socket/hccl_socket.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/socket/hccl_network.cc

    #/src/platform/resource/transport/device
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/device/transport_device_p2p.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/device/transport_device_ibverbs.cc

    #/src/platform/resource/transport/onesided
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/transport_mem.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/transport_roce_mem.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/transport_ipc_mem.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/hccl_mem.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/device/transport_device_roce_mem.cc

    #/src/platform/resource/transport/host
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/transport_ibverbs.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/transport_p2p.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/transport_net.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/transport_tcp.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/transport_roce.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/transport_shm_event.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/transport_virtural.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/transport_direct_npu.cc

    #/src/platform/resource/transport/heterog
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/transport_heterog.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/transport_heterog_roce.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/transport_heterog_event_tcp.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/transport_heterog_event_roce.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/transport_heterog_p2p.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/transport_heterog_raw_roce.cc

    #/src/platform/resource/transport
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/transport_base.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/transport.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/tcp_send_thread_pool.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/mem_name_repository.cc
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/tcp_recv_task.cc

    #/src/platform/debug
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/socket.cc

    ${HCCL_CODE_ROOT}/src/platform/debug/traceinfo/hccl_trace_info.cc

    #/src/platform/task/
    ${HCCL_CODE_ROOT}/src/platform/task/dispatcher.cc
    ${HCCL_CODE_ROOT}/src/platform/task/dispatcher_common.cc
    ${HCCL_CODE_ROOT}/src/platform/task/dispatcher_graph.cc
    ${HCCL_CODE_ROOT}/src/platform/task/dispatcher_virtural.cc
    ${HCCL_CODE_ROOT}/src/platform/task/task_logic_info.cc
    ${HCCL_CODE_ROOT}/src/platform/task/dispatcher_aicpu.cc
    ${HCCL_CODE_ROOT}/src/platform/task/rtsq_interact/aicpu_hccl_sqcq.cc
    ${HCCL_CODE_ROOT}/src/platform/task/rtsq_interact/aicpu_hccl_sqcqv1.cc
    ${HCCL_CODE_ROOT}/src/platform/task/rtsq_interact/aicpu_hccl_sqcqv2.cc

    #/src/platform/typical
    ${HCCL_CODE_ROOT}/src/platform/typical/interface_hccl.cc
    ${HCCL_CODE_ROOT}/src/platform/typical/send_recv_executor.cc
    ${HCCL_CODE_ROOT}/src/platform/typical/typical_mr_manager.cc
    ${HCCL_CODE_ROOT}/src/platform/typical/typical_qp_manager.cc
    ${HCCL_CODE_ROOT}/src/platform/typical/typical_sync_mem.cc
    ${HCCL_CODE_ROOT}/src/platform/typical/typical_window_mem.cc
    ${HCCL_CODE_ROOT}/src/platform/typical/rdma_resource_manager.cc

    #/src/platform/hccl_tbe_task
    ${HCCL_CODE_ROOT}/src/platform/legacy/common/legacy_common.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/graph_ctx_mgr/graph_ctx_mgr_common.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/graph_ctx_mgr/ffts_ctx_provider.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/graph_ctx_mgr/graph_ctx_mgr.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/tbe_common.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/tbe_vector_reduce.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/tbe_crack_cleared.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/eletwise_v1.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/eletwise_v2.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/eletwise_v3.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/eletwise_v4.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/auto_tiling_rt2.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/elewise_memset.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/tbe_gatherv2_aicore.cpp
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/tbe_unsorted_segment_sum_aicore.cpp
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/segment_dsl.cc
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/legacy_log.cc

    #/src/platform/pingmesh
    ${HCCL_CODE_ROOT}/src/platform/ping_mesh/ping_mesh.cc
    ${HCCL_CODE_ROOT}/src/platform/ping_mesh/hccn_rping.cc

    #/src/hccl/hccl/hccl_comm
    ${HCCL_CODE_ROOT}/src/hccd/hccl_comm_conn_mgr.cc
    ${HCCL_CODE_ROOT}/src/hccd/hccl_comm_conn.cc

    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/aicpu_kfc_interface.cc

    #/src/framework/device/aicpu_kfc/algorithm
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/algorithm/aicpu_allgather.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/algorithm/aicpu_allreduce.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/algorithm/aicpu_dispatcher.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/algorithm/task_orchestrator.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/algorithm/aicpu_dmy_cal_allreduce.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/algorithm/aicpu_reduce_scatter.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/algorithm/aicpu_alltoall.cc

    #/src/framework/device/common/
    ${HCCL_CODE_ROOT}/src/framework/device/common/sqe_context_utils.cc

    #/src/framework/device/aicpu_kfc/common/
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/common/aicpu_sqe_context.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/common/aicpu_kfc_utils.cc

    #/src/framework/device/aicpu_kfc/framework/
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/framework/aicpu_kfc_batchwrite_process.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/framework/aicpu_kfc_deprecated_process.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/framework/aicpu_kfc_process.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/framework/aicpu_kfc_prof.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/framework/aicpu_kfc_retry_process.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/framework/aicpu_kfc_rpc_server.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/framework/aicpu_kfc_rpc_serverv2.cc

    #/src/framework/device/aicpu_kfc/dfx/
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/dfx/aicpu_executor_tracer.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/dfx/aicpu_profiling_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/dfx/mc2_trace_utils.cc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/dfx/dfx_extend_info.cc

    #/src/framework/device/debug/dfx/
    ${HCCL_CODE_ROOT}/src/framework/device/debug/dfx/trace/executor_tracer.cc
    ${HCCL_CODE_ROOT}/src/framework/device/debug/dfx/trace/cann_error_reporter.cc
    ${HCCL_CODE_ROOT}/src/framework/device/debug/dfx/profiling/profiling_extend_info.cc
    ${HCCL_CODE_ROOT}/src/framework/device/debug/dfx/profiling/profiling_manager_device.cc

    #/hccl/framework/device/framework/
    ${HCCL_CODE_ROOT}/src/framework/device/framework/aicpu_hccl_process.cc
    ${HCCL_CODE_ROOT}/src/framework/device/framework/aicpu_communicator.cc
    ${HCCL_CODE_ROOT}/src/framework/device/framework/aicpu_one_side_service.cc
    ${HCCL_CODE_ROOT}/src/framework/device/framework/aicpu_hdc.cc
    ${HCCL_CODE_ROOT}/src/framework/device/framework/aicpu_share_data_manager.cc

    #这个文件是软连接过来的
    ${HCCL_CODE_ROOT}/src/framework/device/framework/aicpu_zero_copy_exchanger.cc

    #/hccl/framework/device/utils/
    ${HCCL_CODE_ROOT}/src/framework/device/utils/hccl_aicpu_utils.cc
    ${HCCL_CODE_ROOT}/src/framework/device/utils/aicpu_hdc_utils.cc

    #/hccl/framework/device/
    ${HCCL_CODE_ROOT}/src/framework/device/hccl_aicpu_interface.cc
    ${HCCL_CODE_ROOT}/src/framework/device/hccl_aicpu_transport_interface.cc

    ${HCCL_CODE_ROOT}/src/hccd/hccd.cc
    ${HCCL_CODE_ROOT}/src/hccd/hccl_raw.cc
    ${HCCL_CODE_ROOT}/src/hccd/hccd_comm.cc
    ${HCCL_CODE_ROOT}/src/hccd/hccd_impl_pml.cc

    #/hccl/hccl/hcom
    ${HCCL_CODE_ROOT}/src/framework/hcom/hcom.cc
    ${HCCL_CODE_ROOT}/src/framework/hcom/gradient_segment/gradient_segment.cc

    # hccl/hccl/algorithm/base/communicator
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_nb_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_nhr_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_nhr_v1_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_ahc_transport_req_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_ahc_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_ahc_broke_transport_req.cc

    # hccl/hccl/algorithm/base/alg_template
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/alg_template_register.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/nonuniform_hierarchical_ring_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_nhr.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_nhr.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nhr.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_nhr.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter/scatter_nhr.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce/reduce_nhr_oneshot.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nhr_oneshot.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_nhr_oneshot.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/nonuniform_hierarchical_ring_v1_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_nhr_v1.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_nhr_v1.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nhr_v1.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_nhr_v1.cc

    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_hd_stage.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_local_reduce.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_plant_local_reduce.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_plant_local_reduce_combine.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/nonuniform_bruck_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_hd_stage.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_nb.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_pipeline.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_nb.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nb.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nb_binary.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter/scatter_nb.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter/scatter_ring_direct.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_nb.cc

    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/alg_template_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_nslb.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_alg_template_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_ahc.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_ahc_broke.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_ahc.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_ahc_broke.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ahc.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ahc_broke.cc

    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_reduce_broadcast.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_mesh_opbase.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_chunk_mesh.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_local_reduce.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_local_reduce_bcast.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_mesh_oneshot.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_doubling.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_doubling_direct.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_doubling_local_reduce.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_pipeline.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_opbase_pipeline.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoall/allltoall_pipeline_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoall/allltoall_pipeline_mesh_pairwise_ping_pong.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoall/allltoall_pipeline_mesh_pairwise_ccl_enough.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_direct_fullmesh.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_for_310p.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_hd_optim.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast/broadcast_oneshot.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_unified_march.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_unified_march.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/component/reducer.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/component/sender.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/mc2_handler/mc2_handler.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/alg_env_config.cc
)

set(hccl_list_open
    #framework/cluster_maintenance/health/
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/health/heartbeat/heartbeat.cc
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/detect_connect_anomalies.cc

    #framework/cluster_maintenance/recovery
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry/opretry_base.cc
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry/opretry_agent.cc
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry/opretry_server.cc
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry/opretry_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry/opretry_link_manage.cc
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry/opretry_connection.cc

    #/hccl/platform/common/src
    ${HCCL_CODE_ROOT}/src/framework/common/src/launch_aicpu.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/param_check.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/config.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/json_utils.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/read_write_lock_base.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/read_write_lock.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/config/env_config.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/config/env_config_host.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/hccl_hash_utils.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/task/callback_thread_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/task/hccl_callback_task.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/opexecounter/opexecounter.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/order_launch/order_launch.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_ranktableParser.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_ranktableStandard.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_ranktableConcise.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_ranktableHeterog.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_roletableParser.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_ranktableOffline.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_ranktable_partition.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_exchange_base.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_exchange_agent.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_exchange_dispatcher.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_exchange_server.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_detect.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/topoinfo_parse.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo/hccl_whitelist.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/exception/exception_handler.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/thread/threads_guard.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/onesided_memory_management/global_mem_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/onesided_memory_management/global_mem_record.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/order_launch/order_launch.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/hccl_communicator_attrs.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/hccl_communicator_attrs_host.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/hccl_comm.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/hccl_comm_host.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/comm_config.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/hccl_communicator.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/hccl_communicator_host.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/aclgraph/zero_copy_acl_graph.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/task_abort_handler.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/resource_manager/preempt_port_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/resource_manager/transport_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/one_sided_service/i_hccl_one_sided_service.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/one_sided_service/hccl_one_sided_service.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/one_sided_service/hccl_one_sided_conn.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/one_sided_service/one_sided_service_adapt.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/zero_copy/zero_copy_memory_agent.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/zero_copy/zero_copy_address_mgr.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/zero_copy/zero_copy_address_mgr_host.cc

    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/channel/channel_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/channel/device/channel_aicpu_interface.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/data_api/hccl_api_data.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/data_api/launch_context.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/rank_graph/rank_graph.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/engine/thread_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/engine/notify_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/engine/aicpu_launch_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/engine/comm_engine_res_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/thread/hccl_thread.cc

    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/comm_mem_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/hccl_independent_op_channel.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/hccl_independent_op_ctx.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/hccl_independent_op_mem.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/hccl_independent_op_engine.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/independent_op_context_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/independent_op.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/reg_mem_manager.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/device/engine_aicpu_interface.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/hccl_independent_common.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/engine/notify_manager.cc

    ${HCCL_CODE_ROOT}/src/framework/hcom/hcom_common.cc

    #/hccl/framework/op_base/
    ${HCCL_CODE_ROOT}/src/framework/op_base/src/op_base.cc
    ${HCCL_CODE_ROOT}/src/framework/op_base/src/op_base_mc2.cc
    ${HCCL_CODE_ROOT}/src/framework/op_base/src/op_base_host.cc

    #/hccl/framework/nslbdp
    ${HCCL_CODE_ROOT}/src/framework/nslbdp/hccl_nslbdp.cc
    ${HCCL_CODE_ROOT}/src/framework/nslbdp/hccl_nslb_md5.cc

    # hccl/hccl/algorithm/base/communicator
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/topo_info_extractor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy/comm_factory.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy/comm_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy/comm_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy/comm_star.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy/comm_p2p.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy/comm_mesh.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy/comm_halving_doubling.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_hd_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_mesh_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_p2p_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_partial_mesh_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_ring_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_transport_req_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/calc_hccs_plus_sio_transport_req.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/alg_env_config.cc

    # hccl/hccl/algorithm/base/alg_template
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/aligned_reduce_scatter_double_ring_with_serial_local_copy.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/aligned_reduce_scatter_double_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_halving_doubling.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_mesh.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_mesh_direct.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_mesh_atomic.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_mesh_mix.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/aligned_all_gather_double_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_ring_concurrent_direct.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_ring_direct.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_recursive_hd.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast/bcast_recursive_halvingdoubling.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast/broadcast_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast/broadcast_star.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_gather/gather_star.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/recursive_halvingdoubling_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce/reduce_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_halving_doubling.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_recursive_hd.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ring_concurrent_direct.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_atomic.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_atomic_opbase.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_mix.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_mix_single_stream.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter/scatter_mesh.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter/scatter_ring_concurrent_direct.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter/scatter_double_ring_direct.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter/scatter_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter/multi_root_scatter_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_send_recv/send_receive.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather/all_gather_recursive_hd.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_gather/gather_ring.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_gather/gather_mesh.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce/reduce_recursive_hd.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_pairwise.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_pairwise.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_mesh.cc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_calculator.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_mesh_opbase_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_mesh_executor.cc

    # hccl/hccl/algorithm/impl
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_alg_utils.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/alg_configurator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/hccl_alg.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/hccl_aiv.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/hccl_aiv_host.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/topo_matcher.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/task/parallel_task_loader.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/task/task_loader.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/task/threadManage.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/coll_alg_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/scatter_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/reduce_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/all_reduce_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/reduce_scatter_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/reduce_scatter_v_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/all_gather_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/all_gather_v_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/broadcast_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/send_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/receive_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/batchsendrecv_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/alltoall_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/registry/coll_alg_op_registry.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy/hccl_impl.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy/operator/gather_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy/operator/send_receive_operator.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy/operator/broadcast_operator_for_hetero.cc

    # hccl/hccl/common/health
    ${HCCL_CODE_ROOT}/src/common/health/rank_consistentcy_checker.cc
    ${HCCL_CODE_ROOT}/src/common/health/calc_crc.cc

    # hccl/hccl/common/debug
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/task_profiling.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/task_exception_handler.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/task_exception_handler_host.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/task_overflow.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/plugin_runner.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/plugin_runner_host.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/profiler_base.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/command_handle.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/profiling_manager.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/profiling_manager_pub.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/profiler_manager_impl.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/profiler_manager.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/adapter_prof.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/dlprof_function.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/dlrt_function.cc
    ${HCCL_CODE_ROOT}/src/common/debug/config/config_log.cc

    ${HCCL_CODE_ROOT}/src/common/launch_device/launch_device.cc

    # hccl/hccl/common/stream
    ${HCCL_CODE_ROOT}/src/common/stream/stream_utils.cc

    # hccl/hccl/algorithm/impl/resource_manager
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager/queue_notify_manager.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager/workspace_mem.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager/workspace_resource.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager/workspace_resource_impl.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager/ccl_buffer_manager.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager/offload_stream_manager.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager/op_base_stream_manager.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager/hccl_socket_manager.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager/stream_active_manager.cc

    # hccl/hccl/algorithm/impl/coll_executor
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_executor_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_native_executor_base.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/alg_profiling.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_comm_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/registry/coll_alg_exec_registry.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_all_gather
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_comm_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_aligned_all_gather_double_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_opbase_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_opbase_pipeline_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_single_rank_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_small_count_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mix_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_aiv_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_aiv_smallcount_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_semi_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_aiv_rdma_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_aiv_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/310P/coll_all_gather_for_310p_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/310P/coll_all_gatherv_for_310p_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_mesh_opbase_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gatherv_mesh_aiv_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gatherv_mesh_aiv_smallcount_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_all_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_single_rank_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_small_count_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_comm_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_fast_double_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_aligned_all_reduce_double_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_mid_count_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_oneshot_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_opbase_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_opbase_pipeline_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_reduce_plus_bcast_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_aiv_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_small_count_aiv_rdma_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mid_count_aiv_rdma_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_aiv_smallcount_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mix_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_order_preserved_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/310P/coll_all_reduce_for_310p_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/310P/coll_all_reduce_for_310p_doubling_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/310P/coll_all_reduce_for_310p_doubling_direct_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_aiv_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_smallcount_for_910_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_mesh_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_ring_plus_hd_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_comm_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_single_rank_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_send_receive
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_send_receive/coll_batch_send_recv_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_send_receive/coll_batch_send_recv_retry_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_send_receive/coll_send_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_send_receive/coll_receive_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_reduce_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_comm_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_deter_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_fast_double_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_aligned_reduce_scatter_double_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_dma_elimination.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_opbase_pipeline_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_single_rank_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mix_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_aiv_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_aiv_smallcount_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_semi_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_aiv_rdma_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_order_preserved_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/310P/coll_reduce_scatter_for_310p_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor_host.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_single_rank_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_2level_pipeline_excecutor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_fullmesh_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_staged_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_mesh_aiv_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_direct_fullmesh_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_staged_aiv_rdma_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_comm_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_mesh_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_single_rank_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_ring_for_910_93_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P/coll_reduce_scatter_v_for_310p_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_mesh_opbase_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_aiv_big_count_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_mesh_aiv_smallcount_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_mesh_executor.cc

    # hccl/hccl/algorithm/impl/coll_executor/coll_broadcast
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_ring_for_910_93_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_mesh_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_ring_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_comm_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_mix_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/310P/coll_broadcast_for_310p_comm_executor.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/310P/coll_broadcast_plus_broadcast.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_smallcount_executor.cc
)

add_library(hccl_llt SHARED ${hccl_llt_stub} ${hccl_list})
#头文件搜索路径
target_include_directories(hccl_llt  PRIVATE
    # # inc目录下头文件
    ${TOP_DIR}/inc
    ${TOP_DIR}/metadef/inc
    ${TOP_DIR}/metadef/pkg_inc
    ${TOP_DIR}/metadef/inc/common/util/trace_manager
    ${TOP_DIR}/metadef/inc/common/util/error_manager
    ${TOP_DIR}/metadef/inc/register
    ${TOP_DIR}/metadef/inc/graph
    ${TOP_DIR}/metadef/inc/external
    ${TOP_DIR}/metadef/inc/external/graph
    ${TOP_DIR}/metadef/inc/common
    ${TOP_DIR}/metadef/
    ${TOP_DIR}/runtime/include/external/
    ${TOP_DIR}/inc/external
    ${TOP_DIR}/ace/npuruntime/runtime/platform/inc
    ${TOP_DIR}/ace/npuruntime/inc/
    ${TOP_DIR}/inc/aicpu/
    ${TOP_DIR}/inc/aicpu/tsd
    ${TOP_DIR}/inc/aicpu/aicpu_schedule/aicpu_sharder
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/air/inc/
    ${TOP_DIR}/air/inc/external
    ${TOP_DIR}/air/inc/graph_metadef
    ${TOP_DIR}/air/inc/graph_metadef/external
    ${TOP_DIR}/air/inc/graph_metadef/external/register
    ${TOP_DIR}/air/inc/graph_metadef/graph/utils
    ${TOP_DIR}/air/graph_metadef/
    ${TOP_DIR}/abl/atrace/inc/utrace
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/abl/qos/qosmng
    ${TOP_DIR}/ace/comop/inc
    ${TOP_DIR}/ace/comop/inc/hccl
    ${TOP_DIR}/hcomm/inc/
    ${TOP_DIR}/hcomm/inc/hccl
    ${TOP_DIR}/hcomm/inc/hccl/hccl
    ${TOP_DIR}/hcomm/pkg_inc
    ${TOP_DIR}/hcomm/pkg_inc/hccl
    ${TOP_DIR}/hcomm/src/engines/hcclengine/inc
    ${TOP_DIR}/asl/hss/inc
    ${TOP_DIR}/asl/hss/inc/pub
    ${TOP_DIR}/asl/hss/common/hash_table
    ${TOP_DIR}/inc/aicpu/common
    ${TOP_DIR}/ace/csruntime/inc/tsch
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/include
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl/adv_api
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl/adv_api/detail

    ${HCCL_LLT_ROOT}/stub
    ${TOP_DIR}/hlt/hccl/third_party/mellanox/4.2/include
    ${TOP_DIR}/open_source/json/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/mpi/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include/3rdparty

    ${HCCL_CODE_ROOT}/inc/
    ${HCCL_CODE_ROOT}/inc/hccl/
    ${HCCL_CODE_ROOT}/inc/hccl/hccl
    ${HCCL_CODE_ROOT}/pkg_inc
    ${HCCL_CODE_ROOT}/pkg_inc/hccl
    ${HCCL_CODE_ROOT}/src/pub_inc/
    ${HCCL_CODE_ROOT}/src/pub_inc/inner
    ${HCCL_CODE_ROOT}/src/pub_inc/aicpu
    ${HCCL_CODE_ROOT}/src/pub_inc/new
    ${HCCL_CODE_ROOT}/src/platform/hccp/inc
    ${HCCL_CODE_ROOT}/src/platform/hccp/inc/network
    ${HCCL_CODE_ROOT}/src/pkg_inc
    ${HCCL_CODE_ROOT}/external_depends

    # hccl/hccl目录下头文件
    ${HCCL_CODE_ROOT}/src/inc
    ${HCCL_CODE_ROOT}/src/algorithm/pub_inc
    ${HCCL_CODE_ROOT}/src/algorithm/impl
    ${HCCL_CODE_ROOT}/src/framework/
    ${HCCL_CODE_ROOT}/src/framework/inc/
    ${HCCL_CODE_ROOT}/src/framework/inc/host/
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/health/heartbeat/
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry
    ${HCCL_CODE_ROOT}/src/framework/common/src
    ${HCCL_CODE_ROOT}/src/framework/common/src/host
    ${HCCL_CODE_ROOT}/src/framework/common/src/config
    ${HCCL_CODE_ROOT}/src/framework/common/src/task
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo
    ${HCCL_CODE_ROOT}/src/framework/common/src/exception
    ${HCCL_CODE_ROOT}/src/framework/common/src/thread
    ${HCCL_CODE_ROOT}/src/framework/common/src/onesided_memory_management
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/one_sided_service
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/resource_manager
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/channel/
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/channel/device
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/rank_graph
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/engine
    ${HCCL_CODE_ROOT}/src/framework/hcom/
    ${HCCL_CODE_ROOT}/src/framework/hcom/gradient_segment
    ${HCCL_CODE_ROOT}/src/framework/op_base/src/
    ${HCCL_CODE_ROOT}/src/framework/nslbdp/

    ${HCCL_CODE_ROOT}/src/platform/
    ${HCCL_CODE_ROOT}/src/platform/common/
    ${HCCL_CODE_ROOT}/src/platform/common/unique
    ${HCCL_CODE_ROOT}/src/platform/common/buffer_manager
    ${HCCL_CODE_ROOT}/src/platform/inc
    ${HCCL_CODE_ROOT}/src/platform/inc/adapter
    ${HCCL_CODE_ROOT}/src/platform/inc/adapter/host
    ${HCCL_CODE_ROOT}/src/platform/inc/common/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/device/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/device
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer
    ${HCCL_CODE_ROOT}/src/platform/resource/socket
    ${HCCL_CODE_ROOT}/src/platform/resource/mem
    ${HCCL_CODE_ROOT}/src/platform/task/
    ${HCCL_CODE_ROOT}/src/platform/aiv_communication/
    ${HCCL_CODE_ROOT}/src/hccl
    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/inc
    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/comm/inc
    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/executor/component/inc
    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/impl
    ${HCCL_CODE_ROOT}/src/hdcs
    ${HCCL_CODE_ROOT}/src/hccl/op_base/src
    ${HCCL_CODE_ROOT}/src/hccl_heterog/common
    ${HCCL_CODE_ROOT}/src/hccl_heterog/heterog_cpu
    ${HCCL_CODE_ROOT}/src/hccl_heterog/rpc
    ${HCCL_CODE_ROOT}/src/hccd
    ${HCCL_CODE_ROOT}/src/hcom/src/
    ${HCCL_CODE_ROOT}/src/hcom/src/gradient_segment

    ${HCCL_CODE_ROOT}/src/platform/resource/dispatcher_ctx

    # hccl/common 目录下头文件
    ${HCCL_CODE_ROOT}/src/common/health/
    ${HCCL_CODE_ROOT}/src/common/debug/profiling
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/inc
    ${HCCL_CODE_ROOT}/src/common/debug/config
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/inc/host
    ${HCCL_CODE_ROOT}/src/common/stream/
    ${HCCL_CODE_ROOT}/src/common/launch_device/

    # hccl/algorithm 目录下头文件
    ${HCCL_CODE_ROOT}/src/algorithm/base
    ${HCCL_CODE_ROOT}/src/algorithm/base/inc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_aiv_template
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/inc_all_reduce_deter
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy
    ${HCCL_CODE_ROOT}/src/algorithm/base/mc2_handler
    ${HCCL_CODE_ROOT}/src/algorithm/impl
    ${HCCL_CODE_ROOT}/src/algorithm/impl/inc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy/operator
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager
    ${HCCL_CODE_ROOT}/src/algorithm/impl/task
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/registry
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/registry
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_send_receive
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_scatter

    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoall
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/component
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template
    ${HCCL_CODE_ROOT}/src/algorithm/base/pub_inc

    # hccl/operator目录下头文件
    ${HCCL_CODE_ROOT}/operator

    # /hccl/framework/device/目录下头文件
    ${HCCL_CODE_ROOT}/src/framework/device/
    ${HCCL_CODE_ROOT}/src/framework/device/common
    ${HCCL_CODE_ROOT}/src/framework/device/inc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/inc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/common

    # proto.h头文件
    ${CMAKE_BINARY_DIR}/proto/hccl_llt

    # dump头文件
    ${TOP_DIR}/abl/adump/inc/
    ${TOP_DIR}/abl/adump/inc/adump
    # tbe头文件
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/
    ${HCCL_CODE_ROOT}/src/platform/legacy/common/
    ${HCCL_CODE_ROOT}/src/platform/legacy/graph_ctx_mgr/
    ${HCCL_CODE_ROOT}/src/platform/legacy/inc/
)

add_library(hccl_llt_open SHARED ${hccl_list_open})
#头文件搜索路径
target_include_directories(hccl_llt_open  PRIVATE
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
    ${TOP_DIR}/ace/npuruntime/inc/
    ${TOP_DIR}/metadef/inc/external/graph
    ${TOP_DIR}/runtime/include/external
    ${TOP_DIR}/metadef/inc/common
    ${TOP_DIR}/inc/aicpu/
    ${TOP_DIR}/inc/aicpu/aicpu_schedule/aicpu_sharder
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/metadef/
    ${TOP_DIR}/air/inc/
    ${TOP_DIR}/air/inc/external
    ${TOP_DIR}/air/inc/graph_metadef
    ${TOP_DIR}/air/inc/graph_metadef/external
    ${TOP_DIR}/air/inc/graph_metadef/external/register
    ${TOP_DIR}/air/inc/graph_metadef/graph/utils
    ${TOP_DIR}/air/graph_metadef/
    ${TOP_DIR}/abl/atrace/inc/utrace
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/abl/qos/qosmng
    ${TOP_DIR}/ace/comop/inc
    ${TOP_DIR}/ace/comop/inc/hccl
    ${TOP_DIR}/hcomm/inc
    ${TOP_DIR}/hcomm/inc/hccl
    ${TOP_DIR}/hcomm/inc/hccl/hccl
    ${TOP_DIR}/hcomm/pkg_inc
    ${TOP_DIR}/hcomm/pkg_inc/hccl
    ${TOP_DIR}/hcomm/src/engines/hcclengine/inc
    ${TOP_DIR}/asl/hss/inc
    ${TOP_DIR}/asl/hss/inc/pub
    ${TOP_DIR}/asl/hss/common/hash_table
    ${TOP_DIR}/inc/aicpu/common
    ${TOP_DIR}/ace/csruntime/inc/tsch
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl/adv_api
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl/adv_api/detail

    ${HCCL_LLT_ROOT}/stub
    ${TOP_DIR}/hlt/hccl/third_party/mellanox/4.2/include
    ${TOP_DIR}/open_source/json/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/mpi/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include/3rdparty

    ${HCCL_CODE_ROOT}/src/pub_inc/
    ${HCCL_CODE_ROOT}/src/pub_inc/inner
    ${HCCL_CODE_ROOT}/src/pub_inc/aicpu
    ${HCCL_CODE_ROOT}/src/pub_inc/new
    ${HCCL_CODE_ROOT}/src/platform/hccp/inc
    ${HCCL_CODE_ROOT}/src/platform/hccp/inc/network
    ${HCCL_CODE_ROOT}/src/pkg_inc
    ${HCCL_CODE_ROOT}/external_depends

    # hccl/hccl目录下头文件
    ${HCCL_CODE_ROOT}/src/inc
    ${HCCL_CODE_ROOT}/src/algorithm/pub_inc
    ${HCCL_CODE_ROOT}/src/algorithm/impl
    ${HCCL_CODE_ROOT}/inc/
    ${HCCL_CODE_ROOT}/inc/hccl/
    ${HCCL_CODE_ROOT}/inc/hccl/hccl
    ${HCCL_CODE_ROOT}/pkg_inc
    ${HCCL_CODE_ROOT}/pkg_inc/hccl
    ${HCCL_CODE_ROOT}/src/framework/
    ${HCCL_CODE_ROOT}/src/framework/inc/
    ${HCCL_CODE_ROOT}/src/framework/inc/host
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/health/heartbeat/
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry
    ${HCCL_CODE_ROOT}/src/framework/common/src
    ${HCCL_CODE_ROOT}/src/framework/common/src/host
    ${HCCL_CODE_ROOT}/src/framework/common/src/task
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo
    ${HCCL_CODE_ROOT}/src/framework/common/src/config
    ${HCCL_CODE_ROOT}/src/framework/common/src/exception
    ${HCCL_CODE_ROOT}/src/framework/common/src/thread
    ${HCCL_CODE_ROOT}/src/framework/common/src/onesided_memory_management
    ${HCCL_CODE_ROOT}/src/framework/common/src/order_launch
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/one_sided_service
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/resource_manager
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/channel/
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/channel/device
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/rank_graph
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/engine
    ${HCCL_CODE_ROOT}/src/framework/hcom/
    ${HCCL_CODE_ROOT}/src/framework/hcom/gradient_segment
    ${HCCL_CODE_ROOT}/src/framework/op_base/src/

    ${HCCL_CODE_ROOT}/src/platform/
    ${HCCL_CODE_ROOT}/src/platform/common/
    ${HCCL_CODE_ROOT}/src/platform/common/unique
    ${HCCL_CODE_ROOT}/src/platform/common/buffer_manager
    ${HCCL_CODE_ROOT}/src/platform/inc
    ${HCCL_CODE_ROOT}/src/platform/inc/adapter
    ${HCCL_CODE_ROOT}/src/platform/inc/adapter/host
    ${HCCL_CODE_ROOT}/src/platform/inc/common/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/device/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/device
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer
    ${HCCL_CODE_ROOT}/src/platform/resource/socket
    ${HCCL_CODE_ROOT}/src/platform/resource/mem
    ${HCCL_CODE_ROOT}/src/platform/task/
    ${HCCL_CODE_ROOT}/src/platform/aiv_communication/
    ${HCCL_CODE_ROOT}/src/hccl
    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/inc
    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/comm/inc
    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/executor/component/inc
    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/impl
    ${HCCL_CODE_ROOT}/src/hdcs
    ${HCCL_CODE_ROOT}/src/hccl/op_base/src
    ${HCCL_CODE_ROOT}/src/hccl_heterog/common
    ${HCCL_CODE_ROOT}/src/hccl_heterog/heterog_cpu
    ${HCCL_CODE_ROOT}/src/hccl_heterog/rpc
    ${HCCL_CODE_ROOT}/src/hccd
    ${HCCL_CODE_ROOT}/src/hcom/src/
    ${HCCL_CODE_ROOT}/src/hcom/src/gradient_segment

    # hccl/common 目录下头文件
    ${HCCL_CODE_ROOT}/src/common/health/
    ${HCCL_CODE_ROOT}/src/common/debug/profiling
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/inc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/inc/host
    ${HCCL_CODE_ROOT}/src/common/debug/config
    ${HCCL_CODE_ROOT}/src/common/stream/
    ${HCCL_CODE_ROOT}/src/common/launch_device/

    # hccl/algorithm 目录下头文件
    ${HCCL_CODE_ROOT}/src/algorithm/base
    ${HCCL_CODE_ROOT}/src/algorithm/base/inc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_aiv_template
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/inc_all_reduce_deter
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy
    ${HCCL_CODE_ROOT}/src/algorithm/base/mc2_handler
    ${HCCL_CODE_ROOT}/src/algorithm/impl
    ${HCCL_CODE_ROOT}/src/algorithm/impl/inc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy/operator
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager
    ${HCCL_CODE_ROOT}/src/algorithm/impl/task
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/registry
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/registry
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_send_receive
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_scatter

    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoall
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/inc_all_reduce_deter
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/component
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template
    ${HCCL_CODE_ROOT}/src/algorithm/pub_inc

    # hccl/operator目录下头文件
    ${HCCL_CODE_ROOT}/operator

    # /hccl/framework/device/目录下头文件
    ${HCCL_CODE_ROOT}/src/framework/device/
    ${HCCL_CODE_ROOT}/src/framework/device/common
    ${HCCL_CODE_ROOT}/src/framework/device/inc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/inc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/common

    # proto.h头文件
    ${CMAKE_BINARY_DIR}/proto/hccl_llt
    # dump头文件
    ${TOP_DIR}/abl/adump/inc/
    ${TOP_DIR}/abl/adump/inc/adump
    # tbe头文件
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/
    ${HCCL_CODE_ROOT}/src/platform/legacy/common/
	${HCCL_CODE_ROOT}/src/platform/legacy/graph_ctx_mgr/
	${HCCL_CODE_ROOT}/src/platform/legacy/inc/
)

target_compile_definitions(hccl_llt    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_llt   PRIVATE
     -U_FORTIFY_SOURCE -g
)

target_link_libraries(hccl_llt  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:msprof_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:runtime_headers>
    $<BUILD_INTERFACE:cce_headers>
    $<BUILD_INTERFACE:kernel_tiling_headers>
    c_sec_static
    ascend_protobuf
    mmpa
    -lrt
    -ldl
)

target_compile_definitions(hccl_llt_open    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_llt_open   PRIVATE
     -U_FORTIFY_SOURCE -g
)

target_link_libraries(hccl_llt_open  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:msprof_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:runtime_headers>
    $<BUILD_INTERFACE:cce_headers>
    $<BUILD_INTERFACE:kernel_tiling_headers>
    hccl_llt
    c_sec_static
    ascend_protobuf
    mmpa
    -lrt
    -ldl
)
