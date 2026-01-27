# ccu_device用例应保持独立验证
# 除用例外，其他依赖文件还包含
# ut/common，内含log打桩
# ut/unified_platform/external_system，内含hccp，rts等接口打桩
target_sources(hccl_v2_utest 
    PUBLIC
    ut_ccu_res_specs.cpp
    ut_ccu_component.cpp
    ut_ccu_res_batch_allocator.cpp
    ut_ccu_pfe.cc

    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_device_manager.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_res_specs.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_res_batch_allocator.cpp

    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_component.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_res_allocator.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_eid_info.cpp

    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_channel/ccu_channel_mgr.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_channel/ccu_jetty_ctx_mgr.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_channel/ccu_wqebb_mgr.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_channel/ccu_channel_v1/ccu_channel_mgr_v1.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_channel/ccu_channel_v1/ccu_jetty_ctx_mgr_v1.cpp

    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_pfe/ccu_pfe_mgr.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/ccu_device/ccu_component/ccu_pfe/ccu_pfe_cfg_generator.cpp

    ${HCCL_V2_CODE_ROOT}/unified_platform/common/rdma_handle_manager.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/common/tp_manager.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/external_system/orion_adapter_hccp.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/external_system/orion_adapter_rts.cc
    
    ${HCCL_V2_CODE_ROOT}/unified_platform/trace/trace.cc
    
    ${HCCL_V2_CODE_ROOT}/framework/env_config/env_func.cc
    ${HCCL_V2_CODE_ROOT}/framework/env_config/base_config.cc
    ${HCCL_V2_CODE_ROOT}/framework/env_config/env_config.cc

    ${HCCL_V2_CODE_ROOT}/framework/misc/json_parser/json_parser.cc
    ${HCCL_V2_CODE_ROOT}/framework/misc/whitelist/whitelist.cc
    ${HCCL_V2_CODE_ROOT}/framework/misc/mask_event/mask_event.cc

    ${HCCL_V2_CODE_ROOT}/unified_platform/resource/stream/stream.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/resource/buffer/buffer.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/resource/buffer/local_ub_rma_buffer.cc

    ${HCCL_V2_CODE_ROOT}/unified_platform/common/inner_net_dev_manager.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/common/inner_net_dev.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/common/net_device.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/resource/net_dev/hccl_net_dev_v2.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/resource/net_dev/hccl_net_dev.cc

    ${HCCL_V2_CODE_ROOT}/common/checkcrc.cc
)
