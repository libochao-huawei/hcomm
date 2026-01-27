add_subdirectory(utils)
add_subdirectory(exception)

target_sources(hccl_v2_utest 
    PUBLIC
    slog_stub_ut.cc
    driver_stub.cc
    ut_log.cc
    driver_stub.cc
    ut_binary_stream.cc
    ut_checkcrc.cc
    ut_stream_utils.cc
    ut_ip_address.cc
    ${HCCL_V2_CODE_ROOT}/common/log.cc
    ${HCCL_V2_CODE_ROOT}/common/sal.cc
    ${HCCL_V2_CODE_ROOT}/common/checkcrc.cc
)

