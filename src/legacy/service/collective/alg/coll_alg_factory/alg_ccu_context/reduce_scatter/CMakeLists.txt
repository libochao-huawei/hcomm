target_sources(hccl_v2_native_alg_repo PRIVATE
        reduce_scatter/ccu_context_reduce_scatter_mesh1d.cc
        reduce_scatter/ccu_context_reduce_scatter_mesh1d_2die.cc
        reduce_scatter/ccu_context_reduce_scatter_mesh1d_detour.cc
        reduce_scatter/ccu_context_reduce_scatter_mesh1d_mem2mem.cc
        reduce_scatter/ccu_context_reduce_scatter_mesh2d.cc
        reduce_scatter/ccu_context_reduce_scatter_mesh2d_mem2mem.cc
        reduce_scatter/ccu_context_reduce_scatter_nhr1d_mem2mem.cc
        reduce_scatter/ccu_context_reduce_scatter_v_mesh1d.cc
        reduce_scatter/ccu_context_reduce_scatter_v_mesh1d_mem2mem.cc
)