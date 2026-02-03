# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(report_dir "${OUTPUT_PATH}/report/ut")
# 定义add_run_command函数
function(add_run_command TARGET_NAME TASK_NUM)
    add_test(
        NAME ${TARGET_NAME}
        COMMAND ${CMAKE_COMMAND} -E env
            "LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}:${ASCEND_3RD_LIB_PATH}/gtest_shared/lib/"
            "ASAN_OPTIONS=symbolize=1:detect_leaks=1:halt_on_error=1:verbosity=1:abort_on_error=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_string_checks=1:detect_invalid_pointer_pairs=2"
            $<TARGET_FILE:${TARGET_NAME}>
            --gtest_output=xml:${report_dir}/${TARGET_NAME}.xml
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(${TARGET_NAME} PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}:${ASCEND_3RD_LIB_PATH}/gtest_shared/lib/;ASAN_OPTIONS=symbolize=1:detect_leaks=1:halt_on_error=1:verbosity=1:abort_on_error=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_string_checks=1:detect_invalid_pointer_pairs=2"
    )
    message(STATUS "Registered test: ${TARGET_NAME} (task ${TASK_NUM})")
endfunction()

# 定义run_llt_test函数
function(run_llt_test)
    cmake_parse_arguments(RUN_LLT_TEST "" "TARGET;TASK_NUM" "" ${ARGN})

    message(STATUS "RUN_LLT_TEST_TARGET=${RUN_LLT_TEST_TARGET} ${RUN_LLT_TEST_TASK_NUM}.")
    if(RUN_LLT_TEST_TARGET AND RUN_LLT_TEST_TASK_NUM)
        add_run_command(${RUN_LLT_TEST_TARGET} ${RUN_LLT_TEST_TASK_NUM})
    endif()
endfunction()