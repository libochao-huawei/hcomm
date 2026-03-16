target_sources(hccl_v2_native_alg_repo PRIVATE
        ccu_context_all_reduce_mesh1d.cc
        ccu_context_all_reduce_mesh1d_detour.cc
        ccu_context_all_reduce_mesh1d_mem2mem.cc
        ccu_context_all_reduce_mesh1d_one_shot.cc
        ccu_context_all_reduce_mesh2d_one_shot.cc
        ccu_context_all_reduce_mesh2d_two_shot.cc
        ccu_context_all_reduce_mesh2d_two_shot_mem2mem.cc
        ccu_context_all_reduce_nhr1d_mem2mem.cc
)