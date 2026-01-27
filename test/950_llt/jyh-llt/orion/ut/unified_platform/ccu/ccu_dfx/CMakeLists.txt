target_sources(hccl_v2_utest 
    PUBLIC
    ut_ccu_dfx.cpp
    ut_ccu_error_handler.cpp
    ut_ccu_ra.cpp

    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/dfx/ccu_error_handler.cpp
    ${HCCL_V2_CODE_ROOT}/unified_platform/ccu/dfx/ccu_dfx.cpp
)