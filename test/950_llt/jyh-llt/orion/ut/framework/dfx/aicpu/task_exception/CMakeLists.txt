target_sources(hccl_v2_utest
        PUBLIC
        ut_task_exception_func.cc
        ##ut_task_exception_handler_lite.cc

        ${HCCL_V2_CODE_ROOT}/framework/dfx/aicpu/task_exception/task_exception_func.cpp
        ${HCCL_V2_CODE_ROOT}/framework/dfx/aicpu/task_exception/task_exception_handler_lite.cpp
)