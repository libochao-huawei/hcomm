add_executable(executor_reduce_hccl_test ${src_to_test_list})

#头文件搜索路径
target_include_directories(executor_reduce_hccl_test PRIVATE
    ${HCOMM_ALG_TC_ROOT}/testcase

    ${UTILS_PUBLIC_INCLUDE_LIST}
    ${HCOMM_PUBLIC_INCLUDE_LIST}
    ${HCOMM_PLATFORM_INCLUDE_LIST}
    ${HCOMM_ALGORITHM_INCLUDE_LIST}
    ${HCOMM_COMMON_INCLUDE_LIST}
    ${HCOMM_FRAMEWORK_INCLUDE_LIST}
    ${HCOMM_LEGACY_INCLUDE_LIST}

    ${THIRD_PARTY_PATH}/json/include
    ${THIRD_PARTY_PATH}/gtest/include
    ${THIRD_PARTY_PATH}/mockcpp/include

    ${ASCEND_CANN_INCLUDE_LIST}
)

target_compile_definitions(executor_reduce_hccl_test PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    -DOPEN_HCCL_TEST
)

target_compile_options(executor_reduce_hccl_test PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O0 -g
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)

target_link_libraries(executor_reduce_hccl_test PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    hccl_alg_analyzer
    hccl_alg_adaption
    hccl_alg_test
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
    -pthread
    protobuf
    -lrt
    -ldl
)

add_subdirectory(AllReduceMeshOpbasePipelineExecutor)
add_subdirectory(ReduceScatterMeshOpbasePipelineExecutor)
add_subdirectory(ReduceMeshExecutor)
add_subdirectory(ReduceScatterMeshExecutor)
add_subdirectory(AllReduceMeshExecutor)
add_subdirectory(AllReduceMeshGraphPipelineExecutor)
add_subdirectory(ReduceScatterMeshGraphPipelineExecutor)

target_sources(executor_reduce_hccl_test
    PUBLIC
    main.cc
    ../testcase/testcase_utils.cc
)