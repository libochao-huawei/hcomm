target_sources(hccl_v2 PRIVATE
        alg_template_base_v2.cc
        temp_all_gather_ring.cc
        temp_all_gather_concurr_mesh.cc
        temp_all_gather_mesh.cc
        temp_reduce_scatter_ring.cc
        temp_reduce_scatter_concurr_mesh.cc
        temp_reduce_scatter_mesh.cc
)

if(TARGET ccl_kernel)
target_sources(ccl_kernel PRIVATE
        alg_template_base_v2.cc
        temp_all_gather_ring.cc
        temp_all_gather_concurr_mesh.cc
        temp_all_gather_mesh.cc
        temp_reduce_scatter_ring.cc
        temp_reduce_scatter_concurr_mesh.cc
        temp_reduce_scatter_mesh.cc
)
endif()
