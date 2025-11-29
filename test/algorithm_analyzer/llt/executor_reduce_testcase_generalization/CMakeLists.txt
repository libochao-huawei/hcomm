add_executable(executor_reduce_hccl_test ${src_to_test_list})

#头文件搜索路径
target_include_directories(executor_reduce_hccl_test PRIVATE
    ${package_include_list}
    ${include_list}
    ${algorithm_include_list}
    ${framework_include_list}
    ${common_include_list}
    ${other_include_list}
    ${mockcpp_INCLUDE_DIRS}
)

target_compile_definitions(executor_reduce_hccl_test PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    google=ascend_private
    -DOPEN_HCCL_TEST
    OPEN_BUILD_PROJECT
)

target_compile_options(executor_reduce_hccl_test PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O0 -g
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)

if(BUILD_OPEN_PROJECT)
    target_link_directories(executor_reduce_hccl_test PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/lib
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/lib
    )

    find_library(LIBRARY_ANALYZER libhccl_alg_analyzer.so ${ASCEND_CANN_PACKAGE_PATH}/lib64)
    find_library(LIBRARY_ADAPTION libhccl_alg_adaption.so ${ASCEND_CANN_PACKAGE_PATH}/lib64)
    find_library(LIBRARY_GTEST libgtest.so ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/gtest_shared/lib64)
    target_link_libraries(executor_reduce_hccl_test PRIVATE
        ${LIBRARY_ANALYZER}
        ${LIBRARY_ADAPTION}
        hccl_alg_test
        $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
        ${LIBRARY_GTEST}
        ${LIBRARY_MOCKCPP}
        -lpthread
        ascend_protobuf
        -lrt
        -ldl
    )
elseif(EXISTS "${HCCL_ALG_ANALYZER_LIB_ROOT}/hccl_alg/libhccl_alg_analyzer.so" )
    find_library(LIBRARY_ANALYZER libhccl_alg_analyzer.so ${HCCL_ALG_ANALYZER_LIB_ROOT}/hccl_alg)
    find_library(LIBRARY_ADAPTION libhccl_alg_adaption.so ${HCCL_ALG_ANALYZER_LIB_ROOT}/hccl_alg)
    target_link_libraries(executor_reduce_hccl_test PRIVATE
        ${LIBRARY_ANALYZER}
        ${LIBRARY_ADAPTION}
        hccl_alg_test
        $<BUILD_INTERFACE:intf_llt_pub>
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
    )
else()
    target_link_libraries(executor_reduce_hccl_test PRIVATE
        hccl_alg_analyzer
        hccl_alg_test
        $<BUILD_INTERFACE:intf_llt_pub>
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
    )

    add_dependencies(executor_reduce_hccl_test  hccl_alg_analyzer hccl_alg_test hccl_alg_aiv_test)

endif()

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

if(NOT BUILD_OPEN_PROJECT)
run_llt_test(
   TARGET  executor_reduce_hccl_test
   TASK_NUM 1
)
endif()