set(hccl_ut_prepare_env_list 
    main.cc
)
add_executable(hccl_ut_prepare_env  ${hccl_ut_prepare_env_list})

target_compile_options(hccl_ut_prepare_env   PRIVATE 
    -U_FORTIFY_SOURCE
)

target_link_libraries(hccl_ut_prepare_env PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    -lrt     
)

run_llt_test(
   TARGET  hccl_ut_prepare_env
   TASK_NUM 1
)
