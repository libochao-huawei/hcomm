add_subdirectory(aiv_ins)
add_subdirectory(aiv_mc2)
add_subdirectory(aiv_utils)
target_sources(hccl_v2_utest
        PUBLIC
        ut_aiv_ins_preprocessor.cpp
)