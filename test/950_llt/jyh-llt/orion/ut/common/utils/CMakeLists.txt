target_sources(hccl_v2_utest
        PUBLIC
        hash_utils_test.cc
        ut_string_util.cc

        ${HCCL_V2_CODE_ROOT}/common/utils/hash_utils.cc
)