# HCCL communication task exception test
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/hccl_comm_task_exception_test.cc
)

add_executable(hccl_comm_task_exception_test ${TEST_SOURCES})

target_link_libraries(hccl_comm_task_exception_test
    PUBLIC
        gtest
        gmock
        hccl
        hcomm
        hccl_llt
)

add_test(NAME hccl_comm_task_exception_test
    COMMAND hccl_comm_task_exception_test
)
