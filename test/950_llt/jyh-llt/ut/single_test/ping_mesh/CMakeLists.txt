set(hccl_utest_pingmesh_list
    main.cc
    ut_ping_mesh.cc
    ut_hccn_rping.cc
)
add_executable(hccl_utest_pingmesh  ${hccl_utest_pingmesh_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_pingmesh  PRIVATE
    ${include_list}
 
)
 
target_compile_definitions(hccl_utest_pingmesh    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)
 
target_compile_options(hccl_utest_pingmesh   PRIVATE
    -U_FORTIFY_SOURCE
)
 
target_link_libraries(hccl_utest_pingmesh  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:msprof_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:runtime_headers>
    $<BUILD_INTERFACE:cce_headers>
    $<BUILD_INTERFACE:network_headers>
    $<BUILD_INTERFACE:atrace_headers>
    c_sec_static
    ascend_protobuf
    hccl_llt
    hccl_llt_open
    -lrt
    -ldl
#    exe_graph
)
add_dependencies(hccl_utest_pingmesh hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_pingmesh
   TASK_NUM 1
)