file(GLOB_RECURSE CCU_REP_SOURCES "${HCCL_V2_CODE_ROOT}/unified_platform/ccu/*.cpp")

target_sources(hccl_v2_utest 
    PUBLIC
    ut_ccu_rep_context.cpp
    ut_ccu_rep_translator.cpp
    ut_ccu_context.cpp
    ut_ccu_context_tracker.cpp
    ut_ccu_rep_load.cpp
    ut_ccu_rep_store.cpp
    ut_ccu_dfx.cpp

    ${CCU_REP_SOURCES}
)