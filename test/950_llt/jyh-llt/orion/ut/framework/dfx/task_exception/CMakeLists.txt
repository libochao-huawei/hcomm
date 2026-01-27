target_sources(hccl_v2_utest
        PUBLIC
        ut_task_exception_handler.cc

        ${HCCL_V2_CODE_ROOT}/framework/dfx/task_exception/task_exception_handler.cpp
)