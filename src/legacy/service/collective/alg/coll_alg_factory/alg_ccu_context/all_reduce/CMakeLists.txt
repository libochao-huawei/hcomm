target_sources(hccl_v2_native_alg_repo PRIVATE
        all_reduce/ccu_context_all_reduce_mesh1d.cc
        all_reduce/ccu_context_all_reduce_mesh1d_detour.cc
        all_reduce/ccu_context_all_reduce_mesh1d_mem2mem.cc
        all_reduce/ccu_context_all_reduce_mesh1d_one_shot.cc
        all_reduce/ccu_context_all_reduce_mesh2d_one_shot.cc
        all_reduce/ccu_context_all_reduce_mesh2d_two_shot.cc
        all_reduce/ccu_context_all_reduce_mesh2d_two_shot_mem2mem.cc
        all_reduce/ccu_context_all_reduce_nhr1d_mem2mem.cc
)