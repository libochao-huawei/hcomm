target_sources(hccl_v2_native_alg_repo PRIVATE
        ccu_context_all_gather_mesh1d.cc
        ccu_context_all_gather_mesh1d_2die.cc
        ccu_context_all_gather_mesh1d_detour.cc
        ccu_context_all_gather_mesh1d_mem2mem.cc
        ccu_context_all_gather_mesh1d_mem2mem_with_stride.cc
        ccu_context_all_gather_mesh2d.cc
        ccu_context_all_gather_mesh2d_mem2mem.cc
        ccu_context_all_gather_nhr1d_mem2mem.cc
        ccu_context_all_gather_v_mesh1d.cc
)