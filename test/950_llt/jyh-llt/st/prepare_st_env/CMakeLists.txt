set(hccl_st_prepare_env_list 
    main.cc
)
add_executable(hccl_st_prepare_env  ${hccl_st_prepare_env_list})

target_compile_options(hccl_st_prepare_env   PRIVATE 
    -U_FORTIFY_SOURCE
)

#头文件搜索路径
target_include_directories(hccl_st_prepare_env PRIVATE
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/hccl 
    ${TOP_DIR}/inc/toolchain
    ${TOP_DIR}/abl/libc_sec/include 
    ${HCCL_CODE_ROOT}/inc 
    ${HCCL_CODE_ROOT}/intra/inc 
)

target_link_libraries(hccl_st_prepare_env  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    -lrt
)

run_llt_test(
    TARGET  hccl_st_prepare_env
    TASK_NUM 1
)
