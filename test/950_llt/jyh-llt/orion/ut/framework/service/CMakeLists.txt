target_sources(hccl_v2_utest
    PUBLIC
    ut_coll_service_default_impl.cc
    ut_coll_service_ai_cpu_impl.cc
    ut_connections_builder.cc
    ut_ins_rules.cc
    ut_interpreter.cc
    ut_coll_service_base.cc
)

add_subdirectory(aicpu)
add_subdirectory(one_sided_service)