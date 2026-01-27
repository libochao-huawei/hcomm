add_library(hccl_st_fwk SHARED "")

add_subdirectory(env_stub)
add_subdirectory(hccp_stub)
add_subdirectory(rts_stub)
add_subdirectory(hccl_v2)
add_subdirectory(log_stub)
add_subdirectory(context)
add_subdirectory(ranktable)


target_sources(hccl_st_fwk
    PRIVATE
        hccl_st_situation.cc
        hccl_st_test_case.cc
        hccl_st_prepare_verify.cc
)

target_compile_options(hccl_st_fwk PRIVATE
        -std=c++14
        -Werror
        -fno-common
        -fno-strict-aliasing
        -pipe
        -g
        -O0
)

target_link_libraries(hccl_st_fwk PRIVATE
    c_sec_static
)