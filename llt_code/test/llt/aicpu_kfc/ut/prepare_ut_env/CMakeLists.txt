set(aicpu_kfc_ut_prepare_env_list 
    main.cc
)
add_executable(aicpu_kfc_ut_prepare_env  ${aicpu_kfc_ut_prepare_env_list})

target_compile_options(aicpu_kfc_ut_prepare_env   PRIVATE 
    -U_FORTIFY_SOURCE
)

target_link_libraries(aicpu_kfc_ut_prepare_env PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    -lrt     
)

run_llt_test(
   TARGET  aicpu_kfc_ut_prepare_env
   TASK_NUM 1
)
