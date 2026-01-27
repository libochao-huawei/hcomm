add_subdirectory(system)

target_sources(hccl_v2_utest 
    PUBLIC
    ut_adapter_hccp.cc
    ut_adapter_rts.cc
    ut_adapter_trace.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/external_system/orion_adapter_rts.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/external_system/orion_adapter_hccp.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/external_system/orion_adapter_tsd.cc
    ${HCCL_V2_CODE_ROOT}/unified_platform/external_system/orion_adapter_trace.cc
)
