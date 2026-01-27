target_sources(hccl_v2_utest
        PUBLIC
        ut_one_sided_service_adapt_v2.cc

        ${HCCL_V2_CODE_ROOT}/framework/entrance/one_sided_service/one_sided_service_adapt_v2.cc
)
