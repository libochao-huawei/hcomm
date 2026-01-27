target_sources(hccl_v2_utest
    PUBLIC
    ut_coll_operator.cc
    ut_coll_operator_check.cc
)
# 算法代码修复后再打开
add_subdirectory(alg)
add_subdirectory(instruction)
add_subdirectory(primitive)