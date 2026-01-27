target_sources(hccl_v2_utest
        PUBLIC
        ut_comm_manager.cc

        ${HCCL_V2_CODE_ROOT}/framework/entrance/hcom/hcom_v2.cc
        ${HCCL_V2_CODE_ROOT}/framework/entrance/hcom_comm/comm_manager.cc
        ${HCCL_V2_CODE_ROOT}/framework/entrance/param_check_v2.cc
)