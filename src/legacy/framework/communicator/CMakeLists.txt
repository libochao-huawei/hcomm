target_sources(hccl_v2 PRIVATE
    hccl_communicator.cc
    communicator_impl.cc
    hccl_params_pub.cc
    task_abort_handler.cc
    hdc.cc
    op_params_checker.cc
    diff_rank_updater.cc
    ccu_super_fast_load.cc
)

target_sources(hccl_v2 PRIVATE
    comm_topo_desc.cc
)

if(TARGET ccl_kernel)
target_sources(ccl_kernel PRIVATE
    hccl_params_pub.cc
)
endif()

add_subdirectory(aicpu)
add_subdirectory(hostdpu)
