target_sources(hccl_v2_native_alg_repo PRIVATE

        executor_utils.cc

)
if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
        executor_utils.cc
)
endif()

add_subdirectory(prim_alg_executor)

add_subdirectory(
    ${ORION_REAL_PATH}/../hccl_next/service/collective/alg/coll_alg_factory/alg_executor/ins_alg_executor
    ${CMAKE_CURRENT_BINARY_DIR}/ins_alg_executor
)
