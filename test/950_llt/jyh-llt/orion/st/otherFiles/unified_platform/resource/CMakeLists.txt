add_subdirectory(ccu_transport)
target_sources(hccl_v2_stest
        PUBLIC
        st_socket.cc
)