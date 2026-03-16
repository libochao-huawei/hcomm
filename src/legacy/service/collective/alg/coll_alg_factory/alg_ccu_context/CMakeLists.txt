target_sources(hccl_v2_native_alg_repo PRIVATE
        ccu_context_alg_base.cc
        ccu_context_utils.cc
)
add_subdirectory(all_gather)
add_subdirectory(all_reduce)
add_subdirectory(all_to_all)
add_subdirectory(broadcast)
add_subdirectory(reduce)
add_subdirectory(reduce_scatter)
add_subdirectory(scatter)
