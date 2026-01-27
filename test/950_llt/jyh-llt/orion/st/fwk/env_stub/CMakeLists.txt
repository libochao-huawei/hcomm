target_include_directories(hccl_st_fwk PRIVATE
        ${TOP_DIR}/abl/qos
        ${TOP_DIR}/abl/libc_sec/include
        ${TOP_DIR}/inc/aicpu/tsd
        ${TOP_DIR}/inc/aicpu
        ${TOP_DIR}/abl/libc_sec/include
        ${TOP_DIR}/inc/external
)


target_sources(hccl_st_fwk
        PRIVATE
        qos_stub.cc
        secure_stub.cc
        tsd_stub.cc
        driver_stub.cc    
)