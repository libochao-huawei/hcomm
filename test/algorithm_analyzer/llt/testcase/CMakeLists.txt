add_executable(open_hccl_test ${src_to_test_list})
#头文件搜索路径
target_include_directories(open_hccl_test PRIVATE
    ${package_include_list}
    ${include_list}
    ${algorithm_include_list}
    ${framework_include_list}
    ${common_include_list}
    ${mockcpp_INCLUDE_DIRS}
    ${other_include_list}
)

target_compile_definitions(open_hccl_test PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    google=ascend_private
    -DOPEN_HCCL_TEST
    OPEN_BUILD_PROJECT
)

target_compile_options(open_hccl_test PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O0 -g
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)

if(BUILD_OPEN_PROJECT) # 蓝区构建
    target_link_directories(open_hccl_test PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/lib
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/lib
    )

    find_library(LIBRARY_ANALYZER libhccl_alg_analyzer.so ${ASCEND_CANN_PACKAGE_PATH}/lib64)
    find_library(LIBRARY_ADAPTION libhccl_alg_adaption.so ${ASCEND_CANN_PACKAGE_PATH}/lib64)
    find_library(LIBRARY_GTEST libgtest.so ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/gtest_shared/lib64)
    target_link_libraries(open_hccl_test PRIVATE
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
elseif(NOT BUILD_OPEN_PROJECT AND NOT DEFINED HOST_HCCL_LLT) # toolkit打包触发
    target_link_libraries(open_hccl_test PRIVATE
        hccl_alg_analyzer
        hccl_alg_test
        hccl_alg_adaption
        ofed_headers
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
    )
elseif(EXISTS "${HCCL_ALG_ANALYZER_LIB_ROOT}/hccl_alg/libhccl_alg_analyzer.so" ) #本地共享库构建
    find_library(LIBRARY_ANALYZER libhccl_alg_analyzer.so ${HCCL_ALG_ANALYZER_LIB_ROOT}/hccl_alg)
    find_library(LIBRARY_ADAPTION libhccl_alg_adaption.so ${HCCL_ALG_ANALYZER_LIB_ROOT}/hccl_alg)
    target_link_libraries(open_hccl_test PRIVATE
        ${LIBRARY_ANALYZER}
        ${LIBRARY_ADAPTION}
        hccl_alg_test
        $<BUILD_INTERFACE:intf_llt_pub>
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
    )
else() # 黄区CI构建
    target_link_libraries(open_hccl_test PRIVATE
        hccl_alg_analyzer
        hccl_alg_test
        hccl_alg_adaption
        $<BUILD_INTERFACE:intf_llt_pub>
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
    )
    add_dependencies(open_hccl_test
        hccl_alg_adaption
        insight_adapter_lib
        hccl_alg_analyzer_test
        hccl_alg_analyzer_lib
        hccl_alg_test_lib
        hccl_alg_aiv_test
    )
endif()

target_sources(open_hccl_test
        PUBLIC
        main.cc
        testcase_utils.cc
        testcase_ahc.cc
        testcase_aiv.cc
        testcase_alg_env_config.cc
        testcase_all_reduce.cc
        testcase_temp_register.cc
        testcase_all_gather.cc
        testcase_all_gather_v.cc
        testcase_reduce_scatter.cc
        testcase_reduce_scatter_v.cc
        testcase_all_to_all.cc
        testcase_all_to_allvc.cc
        testcase_all_to_allv.cc
        testcase_send_recv.cc
        testcase_broadcast.cc
        testcase_reduce.cc
        testcase_scatter.cc
        testcase_batchsendrecv.cc
        testcase_all_gather_v_for_310p.cc
        testcase_reduce_scatter_v_for_310p.cc
        testcase_template_base.cc
        testcase_misc.cc
)

