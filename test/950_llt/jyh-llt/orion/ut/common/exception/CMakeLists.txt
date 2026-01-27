target_sources(hccl_v2_utest 
    PUBLIC
    #TODO 暂时关闭此用例：主要功能为比对错误信息，等错误信息稳定后再打开。
    #ut_hccl_exception.cc
    ut_trace.cc
    
    ${HCCL_V2_CODE_ROOT}/common/exception/hccl_exception.cc
    ${HCCL_V2_CODE_ROOT}/common/exception/exception_defination.cc
)