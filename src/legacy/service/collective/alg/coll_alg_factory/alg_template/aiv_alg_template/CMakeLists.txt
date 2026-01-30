target_sources(hccl_v2_native_alg_repo PRIVATE
    aiv_alg_template_base.cc
    aiv_temp_all_reduce_mesh_1D_twoshot.cc
    aiv_temp_scatter_mesh_1D.cc
    aiv_temp_all_gather_mesh_1D.cc
    aiv_temp_all_reduce_mesh_1D_oneshot.cc
    aiv_temp_broadcast_mesh_1D.cc
    aiv_temp_all_to_all_mesh_1D.cc
    aiv_temp_all_to_all_v_mesh_1D.cc
    aiv_temp_reduce_mesh_1D.cc
    aiv_temp_reduce_scatter_mesh_1D.cc
)