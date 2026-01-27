target_include_directories(hccl_st_fwk PRIVATE
        ${TOP_DIR}/drivers/network/inc/network/
        ${TOP_DIR}/inc/external
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/runtime/pkg_inc/runtime
        ${TOP_DIR}/runtime/pkg_inc/
        ${TOP_DIR}/runtime/pkg_inc/profiling
)


target_sources(hccl_st_fwk
        PRIVATE
        hccp_stub.cc
        fake_socket.cc
)