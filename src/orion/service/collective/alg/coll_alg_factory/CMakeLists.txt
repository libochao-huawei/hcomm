add_subdirectory(alg_template)
add_subdirectory(alg_ccu_context)
add_subdirectory(alg_executor)
add_subdirectory(alg_aiv_instruction)

add_subdirectory(
    ${ORION_REAL_PATH}/../hccl_next/service/collective/alg/coll_alg_factory/alg_topo_match
    ${CMAKE_CURRENT_BINARY_DIR}/alg_topo_match
)
add_subdirectory(
    ${ORION_REAL_PATH}/../hccl_next/service/collective/alg/coll_alg_factory/utils
    ${CMAKE_CURRENT_BINARY_DIR}/utils
)
