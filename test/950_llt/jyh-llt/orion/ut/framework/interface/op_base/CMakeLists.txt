target_sources(hccl_v2_utest
        PUBLIC
        ut_op_base_v2.cc

        ${HCCL_V2_CODE_ROOT}/framework/entrance/op_base/op_base_v2.cc
        ${HCCL_V2_CODE_ROOT}/framework/entrance/param_check_v2.cc
)