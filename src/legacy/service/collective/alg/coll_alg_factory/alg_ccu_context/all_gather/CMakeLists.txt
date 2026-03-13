target_sources(hccl_v2_native_alg_repo PRIVATE
        all_gather/ccu_context_all_gather_mesh1d.cc
        all_gather/ccu_context_all_gather_mesh1d_2die.cc
        all_gather/ccu_context_all_gather_mesh1d_detour.cc
        all_gather/ccu_context_all_gather_mesh1d_mem2mem.cc
        all_gather/ccu_context_all_gather_mesh1d_mem2mem_with_stride.cc
        all_gather/ccu_context_all_gather_mesh2d.cc
        all_gather/ccu_context_all_gather_mesh2d_mem2mem.cc
        all_gather/ccu_context_all_gather_nhr1d_mem2mem.cc
        all_gather/ccu_context_all_gather_v_mesh1d.cc
)