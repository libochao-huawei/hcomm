
target_sources(hccl_v2_utest PUBLIC
    ut_rank_graph_builder.cc
)

set(ENV{HCCL_DETOUR} "detour:1")
