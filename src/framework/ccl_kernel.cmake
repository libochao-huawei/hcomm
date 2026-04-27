# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

if((NOT BUILD_OPEN_PROJECT) OR KERNEL_MODE)
    add_library(ccl_kernel SHARED)

    set(CCL_KERNEL_SRC_LIST
        ${HCOMM_DIR}/src/algorithm/impl/alg_env_config.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_alg_utils.cc
        ${HCOMM_DIR}/src/algorithm/impl/hccl_alg_device.cc
        ${HCOMM_DIR}/src/algorithm/impl/hccl_aiv_device.cc
        ${HCOMM_DIR}/src/algorithm/impl/topo_matcher.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/ccl_buffer_manager.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/stream_active_manager.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/workspace_mem.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/workspace_resource.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/workspace_resource_impl.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/op_base_stream_manager.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/offload_stream_manager.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/hccl_socket_manager.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/queue_notify_manager.cc
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager/share_ccl_buffer_manager.cc

        ${HCOMM_DIR}/src/algorithm/base/communicator/legacy/comm_base.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/legacy/comm_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/legacy/comm_p2p.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/legacy/comm_mesh.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/legacy/comm_halving_doubling.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/legacy/comm_star.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_hd_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_mesh_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_partial_mesh_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_ring_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_transport_req_base.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_p2p_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_hccs_plus_sio_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_ahc_broke_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_ahc_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_ahc_transport_req_base.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_nb_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_nhr_transport_req.cc
        ${HCOMM_DIR}/src/algorithm/base/communicator/calc_nhr_v1_transport_req.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/alg_template_register.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/alg_template_base.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/nonuniform_hierarchical_ring_base.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/nonuniform_hierarchical_ring_v1_base.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/nonuniform_bruck_base.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_base.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_alg_template_base.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_base.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/asymmetric_hierarchical_concatenate_nslb.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/recursive_halvingdoubling_base.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/component/reducer.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/component/sender.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/aligned_all_gather_double_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_ring_concurrent_direct.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_halving_doubling.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_mesh.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_mesh_direct.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_mesh_atomic.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_mesh_mix.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_recursive_hd.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_nhr.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_nhr_v1.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_pipeline.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_unified_march.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_nb.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_ahc.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_ahc_broke.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_hd_stage.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_hccs_sio.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather/all_gather_graph_pipeline.cc
        

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_recursive_hd.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_nhr_oneshot.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_nhr.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_nhr_v1.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_ahc.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_ahc_broke.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_nb.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_reduce_broadcast.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_mesh_opbase.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_mesh_oneshot.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_doubling.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_doubling_direct.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_local_reduce_bcast.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_local_reduce.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_chunk_mesh.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_opbase_pipeline.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_hd_optim.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce/all_reduce_graph_pipeline.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoall/allltoall_pipeline_base.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoall/allltoall_pipeline_mesh_pairwise_ping_pong.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoall/allltoall_pipeline_mesh_pairwise_ccl_enough.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoall/alltoall_symmetric_memory.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_pairwise.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_base.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_pairwise.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_mesh.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_staged_calculator.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_for_310p.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_direct_fullmesh.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv/alltoallv_continuous_pipeline.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast/bcast_recursive_halvingdoubling.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast/broadcast_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast/broadcast_star.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nhr_oneshot.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nhr.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nhr_v1.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nb.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast/broadcast_nb_binary.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast/broadcast_oneshot.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce/reduce_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce/reduce_recursive_hd.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce/reduce_nhr_oneshot.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/aligned_reduce_scatter_double_ring_with_serial_local_copy.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/aligned_reduce_scatter_double_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ring_concurrent_direct.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_halving_doubling.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_recursive_hd.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_atomic.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_mix.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_mix_single_stream.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_mesh_atomic_opbase.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_nhr.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_nhr_v1.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_nb.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ahc.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_ahc_broke.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_hd_stage.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_local_reduce.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_pipeline.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_unified_march.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_hccs_sio.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_plant_local_reduce.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter/reduce_scatter_plant_local_reduce_combine.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter/scatter_ring_concurrent_direct.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter/scatter_double_ring_direct.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter/scatter_mesh.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter/scatter_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter/multi_root_scatter_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter/scatter_nhr.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter/scatter_nb.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter/scatter_ring_direct.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_send_recv/send_receive.cc

        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_gather/gather_ring.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_gather/gather_mesh.cc
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_gather/gather_star.cc

        ${HCOMM_DIR}/src/algorithm/base/mc2_handler/mc2_handler.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_executor_base.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_native_executor_base.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_comm_executor.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_single_rank_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_fast_double_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_aligned_all_reduce_double_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_ring_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_comm_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_mid_count_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_oneshot_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_opbase_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_opbase_pipeline_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_graph_pipeline_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mesh_small_count_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_reduce_plus_bcast_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mix_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_ring_zerocopy_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_order_preserved_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_ars_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_mid_count_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/coll_all_reduce_order_preserved_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/310P/coll_all_reduce_for_310p_ring_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/310P/coll_all_reduce_for_310p_doubling_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/310P/coll_all_reduce_for_310p_doubling_direct_executor.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_comm_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ring_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_aligned_all_gather_double_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_opbase_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_opbase_pipeline_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_single_rank_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_small_count_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_semi_ring_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mix_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_hccs_sio_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ring_zerocopy_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ring_zerocopy_exchange_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ring_zerocopy_pipeline_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_ars_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mid_count_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_graph_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/coll_all_gather_mesh_graph_pipeline_executor.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_all_gather_v_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather_v/coll_aligned_all_gather_v_double_ring_for_910_93_executor.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_mesh_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_ring_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_comm_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_smallcount_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_mix_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_broadcast/coll_broadcast_ring_zerocopy_executor.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_mesh_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_ring_plus_hd_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_comm_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce/coll_reduce_single_rank_executor.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_comm_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_deter_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_fast_double_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_aligned_reduce_scatter_double_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_dma_elimination.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mesh_opbase_pipeline_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ring_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_single_rank_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_semi_ring_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_mix_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_hccs_sio_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ring_zerocopy_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ring_zerocopy_exchange_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ring_zerocopy_exchange_pipeline_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_order_preserved_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_ars_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/coll_reduce_scatter_order_preserved_for_910_93_executor.cc
        
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_aiv_big_count_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_mesh_aiv_smallcount_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_mesh_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_mesh_opbase_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_aligned_reduce_scatter_v_double_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/coll_reduce_scatter_v_fast_double_ring_for_910_93_executor.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_ring_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_ring_for_910_93_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_comm_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_mesh_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_scatter/coll_scatter_single_rank_executor.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_send_receive/coll_batch_send_recv_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_send_receive/coll_batch_send_recv_retry_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_send_receive/coll_batch_send_recv_group_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_send_receive/coll_send_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_send_receive/coll_receive_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_send_receive/coll_batch_write_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor_aicpu.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_single_rank_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_fullmesh_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_direct_fullmesh_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_symmetric_memory_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_2level_pipeline_excecutor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/310P/coll_all_to_all_v_for_310p_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_continuous_pipeline_executor.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_v_staged_executor.cc

        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/registry/coll_alg_exec_registry.cc
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/alg_profiling.cc

        ${HCOMM_DIR}/src/algorithm/impl/task/threadManage.cc
        ${HCOMM_DIR}/src/algorithm/impl/task/parallel_task_loader.cc
        ${HCOMM_DIR}/src/algorithm/impl/task/task_loader.cc

        # common
        ${HCOMM_DIR}/src/common/debug/profiling/task_profiling.cc
        ${HCOMM_DIR}/src/common/debug/profiling/task_exception_handler.cc
        ${HCOMM_DIR}/src/common/debug/profiling/profiler_base.cc
        ${HCOMM_DIR}/src/common/debug/profiling/plugin_runner.cc
        ${HCOMM_DIR}/src/common/debug/profiling/plugin_runner_device.cc
        ${HCOMM_DIR}/src/common/debug/profiling/command_handle.cc
        ${HCOMM_DIR}/src/common/debug/profiling/profiling_manager.cc
        ${HCOMM_DIR}/src/common/debug/profiling/task_overflow.cc
        ${HCOMM_DIR}/src/common/debug/profiling/profiler_manager_impl.cc
        ${HCOMM_DIR}/src/common/debug/profiling/profiler_manager.cc
        ${HCOMM_DIR}/src/common/debug/profiling/profiling_manager_pub.cc
        ${HCOMM_DIR}/src/common/debug/profiling/adapter_prof.cc
        ${HCOMM_DIR}/src/common/debug/profiling/dlprof_function.cc
        ${HCOMM_DIR}/src/common/debug/profiling/dlrt_function.cc
        ${HCOMM_DIR}/src/common/debug/config/config_log.cc
        ${HCOMM_DIR}/src/common/health/calc_crc.cc
        ${HCOMM_DIR}/src/common/health/rank_consistentcy_checker.cc
        ${HCOMM_DIR}/src/common/stream/stream_utils_aicpu.cc

        #framework
        ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/hccl_independent_rank_graph.cc
        ${HCOMM_DIR}/src/framework/communicator/impl/one_sided_service/i_hccl_one_sided_service.cc
    )
    target_sources(ccl_kernel PRIVATE
        ${CCL_KERNEL_SRC_LIST}
    )

    target_compile_options(ccl_kernel PRIVATE
        -Werror
        -Wfloat-equal
        -Wall
        -fno-common
        -fstack-protector-strong
        -fno-strict-aliasing
        -pipe
        -O3
        -std=c++14
    )

    target_link_options(ccl_kernel PRIVATE
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
        -s
    )

    target_compile_definitions(ccl_kernel PRIVATE
        HCCD
        CCL_KERNEL_AICPU
    )

    set(CCL_KERNEL_INCLUDE_LIST
        ${HCOMM_DIR}/include/hccl
        ${HCOMM_DIR}/src/pub_inc
        ${TOP_DIR}/abl/atrace/inc/utrace
        ${HCOMM_DIR}/src/algorithm/base/inc
        ${HCOMM_DIR}/src/algorithm/pub_inc
        ${HCOMM_DIR}/src/algorithm/base/alg_aiv_template
        ${HCOMM_DIR}/src/algorithm/base/alg_template
        ${HCOMM_DIR}/src/algorithm/base/alg_template/component
        ${HCOMM_DIR}/src/algorithm/base/mc2_handler
        ${HCOMM_DIR}/src/algorithm/base/communicator
        ${HCOMM_DIR}/src/algorithm/impl
        ${HCOMM_DIR}/src/algorithm/impl/inc
        ${HCOMM_DIR}/src/algorithm/impl/legacy
        ${HCOMM_DIR}/src/algorithm/impl/legacy/operator
        ${HCOMM_DIR}/src/algorithm/base/communicator/legacy
        ${HCOMM_DIR}/src/algorithm/impl/resource_manager
        ${HCOMM_DIR}/src/algorithm/impl/task
        ${HCOMM_DIR}/src/algorithm/impl/operator
        ${HCOMM_DIR}/src/algorithm/impl/operator/registry
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/registry
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_send_receive
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/310P
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/310P
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather_v
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/310P
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all
        ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_scatter
        ${HCOMM_DIR}/src/common/health
        ${HCOMM_DIR}/src/common/debug/profiling/inc
        ${HCOMM_DIR}/src/common/debug/profiling/inc/aicpu
        ${HCOMM_DIR}/src/common/debug/config/
        ${HCOMM_DIR}/src/common/debug/
        ${HCOMM_DIR}/src/common/stream
        ${HCOMM_DIR}/src/common/launch_device
        ${HCOMM_DIR}/src/framework/inc
        ${HCOMM_DIR}/src/framework/cluster_maintenance/health/heartbeat/
        ${HCOMM_DIR}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/
        ${HCOMM_DIR}/src/framework/cluster_maintenance/snapshot/
        ${HCOMM_DIR}/src/framework/common/src
        ${HCOMM_DIR}/src/framework/common/src/aicpu
        ${HCOMM_DIR}/src/framework/common/src/config
        ${HCOMM_DIR}/src/framework/common/src/task
        ${HCOMM_DIR}/src/framework/common/src/topo
        ${HCOMM_DIR}/src/framework/hcom
        ${HCOMM_DIR}/src/framework/communicator/impl/
        ${HCOMM_DIR}/src/framework/communicator/impl/one_sided_service/
        ${HCOMM_DIR}/src/framework/communicator/impl/zero_copy
        ${HCOMM_DIR}/src/framework/communicator/impl/resource_manager
        ${HCOMM_DIR}/src/framework/op_base/src/
        ${HCOMM_DIR}/src/framework/device/
        ${HCOMM_DIR}/src/framework/device/aicpu_kfc
        ${HCOMM_DIR}/src/framework/device/aicpu_kfc/inc
        ${HCOMM_DIR}/src/framework/device/aicpu_kfc/dfx
        ${HCOMM_DIR}/src/framework/device/common
        ${HCOMM_DIR}/src/framework/device/inc
        ${HCOMM_DIR}/src/framework/device/framework
        ${HCOMM_DIR}/src/framework/cluster_maintenance/recovery/operator_retry
        ${HCOMM_DIR}/src/framework/common/src/exception
        ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/
        ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/resource
        ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/thread
        ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/rank_graph
        ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/channel
        ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/channel/device
        ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/data_api
    
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoall
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter
        ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter
        ${HCOMM_DIR}/src/algorithm/base/alg_template
        ${HCOMM_DIR}/src/algorithm/pub_inc
        ${HCOMM_DIR}/pkg_inc/
        ${HCOMM_DIR}/pkg_inc/hccl/

        ${HCOMM_DIR}/src/platform/
        ${HCOMM_DIR}/src/platform/inc/
        ${HCOMM_DIR}/src/platform/inc/adapter
        ${HCOMM_DIR}/src/platform/resource/transport/heterog/
        ${HCOMM_DIR}/src/platform/resource/transport/host/
        ${HCOMM_DIR}/src/platform/resource/transport/
        ${HCOMM_DIR}/src/platform/resource/notify/
        ${HCOMM_DIR}/src/platform/task
        ${HCOMM_DIR}/src/platform/common
        ${HCOMM_DIR}/src/platform/common/unique
        ${HCOMM_DIR}/src/platform/common/unfold_cache

        ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/ # c_sec、mmpa、unified_dlog动态库搜索路径

        ${RDMA_CORE_INCLUDE_DIR}
        ${HCOMM_DIR}/src/platform/
        ${HCOMM_DIR}/src/platform/inc/
        ${HCOMM_DIR}/src/platform/inc/adapter
        ${HCOMM_DIR}/src/platform/resource/transport/heterog/
        ${HCOMM_DIR}/src/platform/resource/transport/host/
        ${HCOMM_DIR}/src/platform/resource/transport/
        ${HCOMM_DIR}/src/platform/resource/notify/
        ${HCOMM_DIR}/src/platform/resource/dispatcher_ctx
        ${HCOMM_DIR}/src/platform/task
        ${HCOMM_DIR}/src/platform/common
        ${HCOMM_DIR}/src/platform/common/unique
        ${hccl_include_list}
        ${HCOMM_DIR}/include
        ${HCOMM_DIR}/include/hccl
        ${HCOMM_DIR}/inc/hccl/hccl
        ${HCOMM_DIR}/pkg_inc/
        ${HCOMM_DIR}/pkg_inc/hccl/
        ${HCOMM_DIR}/src/pub_inc
        ${HCOMM_DIR}/src/pub_inc/aicpu

        ${HCOMM_DIR}/src/framework/device/
        ${HCOMM_DIR}/src/framework/device/common
        ${HCOMM_DIR}/src/framework/device/inc
        ${HCOMM_DIR}/src/framework/device/framework

        # runtime头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/

        # mmpa头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/acl/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/mmpa/

        # 包间接口
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/common/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/

        # driver头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver

        # ascendc
        ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/highlevel_api/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
        ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/

        ${HCOMM_DIR}/externel_depends/tsch/
        ${RDMA_CORE_INCLUDE_DIR}
        ${THIRD_PARTY_NLOHMANN_PATH}

        ${HCOMM_DIR}/src/platform/hccp/inc/network/
        ${HCOMM_DIR}/src/platform/hccp/inc/
        ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/sockets/
        ${HCOMM_DIR}/src/framework/next/comms/endpoints/
        ${HCOMM_DIR}/src/framework/next/comms/comm_engine_res/threads
        ${HCOMM_DIR}/src/framework/next/comms/comm_engine_res/threads/slaves/
        ${HCOMM_DIR}/src/framework/next/coll_comms/communicator/aicpu
        ${HCOMM_DIR}/src/framework/next/coll_comms/dfx
    )

    target_include_directories(ccl_kernel PRIVATE
        ${CCL_KERNEL_INCLUDE_LIST}
    )

    target_include_directories(ccl_kernel PRIVATE ${ORION_HEAD_LIST})

    add_subdirectory(device)
endif()

if(NOT BUILD_OPEN_PROJECT)
    target_include_directories(ccl_kernel PRIVATE
        ${HCOMM_DIR}/include/hccl
        ${HCOMM_DIR}/src/pub_inc
        ${HCOMM_DIR}/src/pub_inc/inner
        ${HCOMM_DIR}/src/pub_inc/aicpu
        ${HCOMM_DIR}/src/pub_inc/new

        ${HCOMM_DIR}/src/hccl/hccl_comm/wrapper/notify/

        ${TOP_DIR}/inc
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/metadef/inc/external
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/open_source/json/include
        ${TOP_DIR}/inc/aicpu/
        ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api
        ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl
        ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/include
        ${TOP_DIR}/abl/atrace/inc/utrace
        ${TOP_DIR}/runtime/include/external
 	    ${TOP_DIR}/runtime/include/external/acl
 	    ${TOP_DIR}/runtime/pkg_inc
 	    ${TOP_DIR}/runtime/pkg_inc/runtime
 	    ${TOP_DIR}/runtime/pkg_inc/profiling
 	    ${TOP_DIR}/runtime/pkg_inc/trace
 	    ${TOP_DIR}/runtime/pkg_inc/base
 	    ${TOP_DIR}/runtime/pkg_inc/aicpu_sched
        ${TOP_DIR}/asc/asc-devkit
 	    ${TOP_DIR}/asc/asc-devkit/include/adv_api/hccl/internal

        # orion
        ${TOP_DIR}/ace/comop/inc

        ${HCOMM_DIR}/src/platform/
        ${HCOMM_DIR}/src/platform/inc/
        ${HCOMM_DIR}/src/platform/inc/adapter
        ${HCOMM_DIR}/src/platform/resource/transport/heterog/
        ${HCOMM_DIR}/src/platform/resource/transport/host/
        ${HCOMM_DIR}/src/platform/resource/transport/
        ${HCOMM_DIR}/src/platform/resource/notify/
        ${HCOMM_DIR}/src/platform/resource/dispatcher_ctx
        ${HCOMM_DIR}/src/platform/task
        ${HCOMM_DIR}/src/platform/common
        ${HCOMM_DIR}/src/platform/common/unique
        ${HCOMM_DIR}/src/platform/common/unfold_cache

        ${HCOMM_DIR}/src/platform/hccp/inc
        ${HCOMM_DIR}/src/platform/hccp/inc/network
	    ${HCOMM_DIR}/pkg_inc/
        ${HCOMM_DIR}/pkg_inc/hccl/

        ${HCOMM_DIR}/src/framework
 	    ${HCOMM_DIR}/src/framework/common
 	    ${HCOMM_DIR}/src/framework/common/src
 	    ${HCOMM_DIR}/src/framework/common/src/mgr
 	    ${HCOMM_DIR}/src/framework/communicator
 	    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/resource/engine
    )
    target_link_libraries(ccl_kernel PRIVATE
        $<BUILD_INTERFACE:intf_pub_cxx14>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:msprof_headers>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:hccl_headers>
        $<BUILD_INTERFACE:npu_runtime_headers>
        $<BUILD_INTERFACE:atrace_headers>
        $<BUILD_INTERFACE:kernel_tiling_headers>
        -Wl,--no-as-needed
        c_sec
        ccl_kernel_plf
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    )

    install(TARGETS ccl_kernel LIBRARY
        DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
    )
endif()

if(BUILD_OPEN_PROJECT AND KERNEL_MODE)
    if(BUILD_OPEN_PROJECT)
        set(CMAKE_CXX_FLAGS "")
        target_compile_definitions(ccl_kernel PRIVATE
            OPEN_BUILD_PROJECT
            -D_GLIBCXX_USE_CXX11_ABI=1) # 链接ascendalog需要LOG_CPP，链接slog不需要
        # set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--allow-multiple-definition")
        message(STATUS "framework CMAKE_HOST_SYSTEM_PROCESSOR=${CMAKE_HOST_SYSTEM_PROCESSOR}")
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
            message(STATUS "Host is x86_64")
            set(ASCEND_CANN_PACKAGE_ENV_PATH ${ASCEND_CANN_PACKAGE_PATH}/x86_64-linux)
        elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64")
            message(STATUS "Host is ARM64")
            set(ASCEND_CANN_PACKAGE_ENV_PATH ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux)
        endif()
        message(STATUS "framework ASCEND_CANN_PACKAGE_ENV_PATH=${ASCEND_CANN_PACKAGE_ENV_PATH}")
        set(CCL_KERNEL_PLF_PATH
            -Wl,--whole-archive
            ccl_kernel_plf_a
            -Wl,--no-whole-archive
            -Wl,-Bsymbolic
        )
        if(FULL_MODE)
            set(CCL_KERNEL_PLF_PATH ccl_kernel_plf)
        endif()
        target_link_libraries(ccl_kernel PRIVATE
            -Wl,--no-as-needed
            ascend_hal
            c_sec
            mmpa
            ${CCL_KERNEL_PLF_PATH}
            -Wl,--as-needed
            -lrt
            -ldl
            -lpthread
        )

        target_link_directories(ccl_kernel PRIVATE
            ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/ # c_sec、mmpa、unified_dlog动态库搜索路径
        )
    endif()
endif()

if(DEVICE_MODE AND KERNEL_MODE)
    set(CCL_KERNEL_TAR_DIR ${BUILD_DEVICE_DIR}/ccl_kernel_tar_pkg/aicpu_kernels_device)
    add_custom_command(
        TARGET ccl_kernel
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CCL_KERNEL_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ccl_kernel> ${CCL_KERNEL_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ccl_kernel_plf> ${CCL_KERNEL_TAR_DIR}
        COMMAND chmod 750 ${CCL_KERNEL_TAR_DIR}/lib*
        COMMENT "Copying libccl_kernel_plf.so libccl_kernel.so to ${CCL_KERNEL_TAR_DIR}"
    )

    set(AICPU_CUSTOM_COMPILE_DEFINITIONS
        HCCD
        CCL_KERNEL_AICPU
        OPEN_BUILD_PROJECT
        -D_GLIBCXX_USE_CXX11_ABI=1
    )
    set(AICPU_CUSTOM_LINK_LIBRARIES
        -Wl,--no-as-needed
        ascend_hal
        c_sec
        mmpa
        -Wl,--whole-archive
        ccl_kernel_plf_a
        -Wl,--no-whole-archive
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )

    add_library(aicpu_custom SHARED)
    get_target_property(CCL_KERNEL_ALL_SOURCES ccl_kernel SOURCES)
    if(CCL_KERNEL_ALL_SOURCES)
        list(REMOVE_DUPLICATES CCL_KERNEL_ALL_SOURCES)
        target_sources(aicpu_custom PRIVATE ${CCL_KERNEL_ALL_SOURCES})
    endif()
    target_compile_definitions(aicpu_custom PRIVATE ${AICPU_CUSTOM_COMPILE_DEFINITIONS})
    target_compile_options(aicpu_custom PRIVATE
        -Werror
        -Wfloat-equal
        -Wall
        -fno-common
        -fstack-protector-strong
        -fno-strict-aliasing
        -pipe
        -O3
        -std=c++14
    )
    target_link_options(aicpu_custom PRIVATE
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
        -s
    )
    target_include_directories(aicpu_custom PRIVATE
        ${CCL_KERNEL_INCLUDE_LIST}
        ${ORION_HEAD_LIST}
    )
    target_link_directories(aicpu_custom PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/
    )
    target_link_libraries(aicpu_custom PRIVATE ${AICPU_CUSTOM_LINK_LIBRARIES})
    add_dependencies(aicpu_custom ccl_kernel ccl_kernel_plf_a aicpu_custom_json)
    set_target_properties(aicpu_custom PROPERTIES
        OUTPUT_NAME aicpu_custom
        PREFIX "lib"
        SUFFIX ".so"
    )
    install(TARGETS aicpu_custom
        LIBRARY DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
        DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
    )
endif()

if(BUILD_OPEN_PROJECT AND DEVICE_MODE AND KERNEL_MODE)
    set(CCL_KERNEL_TAR_LIBS
        ${BUILD_DEVICE_DIR}/ccl_kernel_tar_pkg/aicpu_kernels_device/libccl_kernel_plf.so
        ${BUILD_DEVICE_DIR}/ccl_kernel_tar_pkg/aicpu_kernels_device/libccl_kernel.so
    )
    pack_targets_and_files(
        OUTPUT_TARGET "generate_device_aicpu_package"
        OUTPUT "aicpu_hcomm.tar.gz"
        FILES ${CCL_KERNEL_TAR_LIBS}
        MANIFEST "bin_hash.cfg"
        TAR_ROOT_DIR "aicpu_kernels_device"
    )
    add_dependencies(generate_device_aicpu_package ccl_kernel ccl_kernel_plf)
    sign_file(
        INPUT "aicpu_hcomm.tar.gz"
        CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/scripts/sign/hcomm_check_cfg.xml"
        RESULT_VAR "aicpu_hcomm_sign_file"
        DEPENDS generate_device_aicpu_package
    )
    add_dependencies(sign_aicpu_hcomm generate_device_aicpu_package)
endif()

# 生成算子信息库
if(BUILD_OPEN_PROJECT AND (NOT KERNEL_MODE))
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
        COMMAND ${HI_PYTHON} ${HCOMM_DIR}/cmake/scripts/parser_ini.py
                             ${CMAKE_CURRENT_SOURCE_DIR}/device/framework/ccl_kernel.ini
                             ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
        DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/config OPTIONAL
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
        COMMAND ${HI_PYTHON} ${HCOMM_DIR}/cmake/scripts/parser_ini.py
                             ${CMAKE_CURRENT_SOURCE_DIR}/device/framework/aicpu_custom.ini
                             ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target(aicpu_custom_json DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
        DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
    )
endif()

# 安装
if(BUILD_OPEN_PROJECT AND (NOT DEVICE_MODE))
    install(FILES ${BUILD_DEVICE_DIR}/src/framework/libccl_kernel.so
        DESTINATION ${CMAKE_SYSTEM_PROCESSOR}-linux/devlib/device OPTIONAL
    )
    install(FILES ${BUILD_DEVICE_DIR}/signatures/aicpu_hcomm.tar.gz
        DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
    )
    install(FILES ${BUILD_DEVICE_DIR}/src/framework/libaicpu_custom.so
        DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
    )
    install(FILES ${BUILD_DEVICE_DIR}/src/framework/libaicpu_custom.json
        DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
    )
endif()
