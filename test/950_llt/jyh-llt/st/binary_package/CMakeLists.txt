set(HCCL_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole)
set(HCCL_OPEN_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/open_source)
set(HCCL_TOOLS_INSIGHT ${TOP_DIR}/asl/devtools/hccl_tools/hccl_insight)
set(HCCL_INSIGHT_DIR ${TOP_DIR}/asl/devtools/hccl_tools/hccl_insight/bs_engine/server)
 
set(hccl_st_binary_package_list 
    main.cc
)
add_executable(hccl_st_binary_package  ${hccl_st_binary_package_list})

target_compile_options(hccl_st_binary_package   PRIVATE 
    -U_FORTIFY_SOURCE
)

#头文件搜索路径
target_include_directories(hccl_st_binary_package PRIVATE
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/hccl 
    ${TOP_DIR}/inc/toolchain
    ${TOP_DIR}/abl/libc_sec/include 
    ${HCCL_CODE_ROOT}/inc 
    ${HCCL_CODE_ROOT}/intra/inc 
)

target_link_libraries(hccl_st_binary_package  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    -lrt
)

run_llt_test(
    TARGET  hccl_st_binary_package
    TASK_NUM 1
)

# 通过hccl_insight_package编译，启动hccl_insight工具的二进制打包
add_custom_command(TARGET hccl_st_binary_package POST_BUILD
  COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/hccl_lib
  COMMAND cd ${HCCL_TOOLS_INSIGHT}/bs_engine && ./hccl_insight_tool_run.sh -env_config -action package
  COMMAND cp ${HCCL_INSIGHT_DIR}/hccl_insight ${CMAKE_INSTALL_PREFIX}/hccl_lib
)