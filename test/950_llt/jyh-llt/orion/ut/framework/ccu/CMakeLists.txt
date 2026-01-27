add_subdirectory(ccu_ins)
add_subdirectory(ccu_manager)
add_subdirectory(ccu_mc2)

target_sources(hccl_v2_utest
        PUBLIC
        ut_ccu_ins_preprocessor.cpp
        ut_ccu_communicator.cpp
)