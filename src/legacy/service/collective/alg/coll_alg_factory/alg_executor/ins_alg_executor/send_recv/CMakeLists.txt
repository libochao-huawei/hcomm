target_sources(hccl_v2_native_alg_repo PRIVATE
        ins_batchSendRecv_executor.cc
        ins_recv_executor.cc
        ins_send_executor.cc
        ins_v2_send_executor.cc
        ins_v2_recv_executor.cc
        ins_v2_batchSendRecv_executor.cc
)

if((NOT BUILD_OPEN_PROJECT) OR DEVICE_MODE)
target_sources(ccl_kernel PRIVATE
        ins_batchSendRecv_executor.cc
        ins_recv_executor.cc
        ins_send_executor.cc
)
endif()