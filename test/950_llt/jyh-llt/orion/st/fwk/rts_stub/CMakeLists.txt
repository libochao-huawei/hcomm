target_include_directories(hccl_st_fwk PUBLIC
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/runtime/pkg_inc/runtime
        ${TOP_DIR}/runtime/pkg_inc/
        ${TOP_DIR}/runtime/pkg_inc/profiling
        ${TOP_DIR}/ace/npuruntime/inc
        ${TOP_DIR}/ace/npuruntime/acl/inc/external/
        ${TOP_DIR}/ace/npuruntime/acl/inc/external/acl
        ${TOP_DIR}/ace/comop/inc/
        ${TOP_DIR}/ace/comop/inc/hccl/
        ${TOP_DIR}/ace/comop/inc/external/
        ${TOP_DIR}/ace/comop/inc/external/hccl
        ${TOP_DIR}/inc/external
        ${TOP_DIR}/abl/adump/inc/adump
        ${TOP_DIR}/metadef/inc/external
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/abl/mmpa/inc/mmpa
        ${TOP_DIR}/abl/mmpa/inc/mmpa/sub_inc
)

target_sources(hccl_st_fwk
        PRIVATE
        rts_stub.cc
        FakeStreamMgr.cc
        mmpa_stub_st.cc
)