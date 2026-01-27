# 添加 hccl_next 算法的 cpp 文件
FILE(GLOB_RECURSE hccl_next_alg_files
    ${HCCL_NEXT_CODE_ROOT}/service/collective/*.cc
    ${HCCL_NEXT_CODE_ROOT}/service/collective/*.cpp
)
FILE(GLOB_RECURSE hccl_next_alg_files_exclude
    ${HCCL_NEXT_CODE_ROOT}/service/collective/alg/coll_alg_factory/utils/host/*.cc
)
LIST (REMOVE_ITEM hccl_next_alg_files ${hccl_next_alg_files_exclude})

# 添加 hccl_orion 算法的 cpp
FILE (GLOB_RECURSE hccl_orion_alg_files
    ${HCCL_V2_CODE_ROOT}/service/collective/alg/coll_alg_factory/alg_ccu_context/*.cc
    ${HCCL_V2_CODE_ROOT}/service/collective/alg/coll_alg_factory/alg_template/ccu_alg_template/*.cc
    ${HCCL_V2_CODE_ROOT}/service/collective/alg/coll_alg_factory/alg_executor/prim_alg_executor/*.cc
    ${HCCL_V2_CODE_ROOT}/service/collective/alg/coll_alg_factory/alg_template/prim_alg_template/*.cc
    ${HCCL_V2_CODE_ROOT}/service/collective/alg/coll_alg_factory/alg_template/aiv_alg_template/*.cc
    ${HCCL_V2_CODE_ROOT}/service/collective/primitive/*.cc
)

target_sources(hccl_v2_utest
        PUBLIC
        ut_ccu_alg_2d.cc
        ${hccl_next_alg_files}
        ${hccl_orion_alg_files}
)
