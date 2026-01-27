
target_sources(hccl_v2_utest
    PUBLIC
    ut_ccu_registered_ctx_mgr.cpp
    ut_ccu_respack_mgr.cpp
    ut_ccu_jetty_mgr.cpp
    ut_ccu_transport_manager.cpp
    ut_ccu_transport_group_manager.cpp

    ${HCCL_V2_CODE_ROOT}/framework/ccu/ccu_manager/ccu_jetty_mgr.cpp
)