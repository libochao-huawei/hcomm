add_subdirectory(ccu_ins)
add_subdirectory(ccu_manager)
add_subdirectory(ccu_mc2)
target_sources(hccl_v2_stest
        PUBLIC
        st_ccu_ins_preprocessor.cpp
        st_ccu_communicator.cpp
)