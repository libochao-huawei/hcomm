# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set_source_files_properties(
    dynamic_add_int32_v80.cce
    dynamic_maximum_float16_v80.cce
    dynamic_maximum_float32_v80.cce
    dynamic_maximum_int32_v80.cce
    dynamic_minimum_float16_v80.cce
    dynamic_minimum_float32_v80.cce
    dynamic_minimum_int32_v80.cce
    dynamic_mul_int32_v80.cce
    add_Ascend910_int8_210000000.cce
    add_Ascend910_int8_210010000.cce
    add_Ascend910_int8_223000000.cce
    add_Ascend910_int8_232000000.cce
    mul_Ascend910_int8_210000000.cce
    mul_Ascend910_int8_210010000.cce
    mul_Ascend910_int8_223000000.cce
    mul_Ascend910_int8_232000000.cce
    add_Ascend910_float16_210000000.cce
    add_Ascend910_float16_210010000.cce
    add_Ascend910_float16_223000000.cce
    add_Ascend910_float16_223010000.cce
    add_Ascend910_float16_232000000.cce
    add_Ascend910_float16_232010000.cce
    mul_Ascend910_float16_210000000.cce
    mul_Ascend910_float16_210010000.cce
    mul_Ascend910_float16_223000000.cce
    mul_Ascend910_float16_223010000.cce
    mul_Ascend910_float16_232000000.cce
    mul_Ascend910_float16_232010000.cce
    mul_Ascend910_float32_210000000.cce
    mul_Ascend910_float32_210010000.cce
    mul_Ascend910_float32_223000000.cce
    mul_Ascend910_float32_223010000.cce
    mul_Ascend910_float32_232000000.cce
    mul_Ascend910_float32_232010000.cce
    maximum_Ascend910_int8_210000000.cce
    maximum_Ascend910_int8_210010000.cce
    maximum_Ascend910_int8_223000000.cce
    maximum_Ascend910_int8_232000000.cce
    minimum_Ascend910_int8_210000000.cce
    minimum_Ascend910_int8_210010000.cce
    minimum_Ascend910_int8_223000000.cce
    minimum_Ascend910_int8_232000000.cce
    add_Ascend910_int64_210000000.cce
    add_Ascend910_int64_210010000.cce
    add_Ascend910_int64_2147483647.cce
    max_Ascend910_int64_210000000.cce
    max_Ascend910_int64_210010000.cce
    max_Ascend910_int64_2147483647.cce
    min_Ascend910_int64_210000000.cce
    min_Ascend910_int64_210010000.cce
    min_Ascend910_int64_2147483647.cce
    mul_Ascend910_int64_210000000.cce
    mul_Ascend910_int64_210010000.cce
    mul_Ascend910_int64_2147483647.cce
    MemSet_dynamic_AtomicAddrClean_1_ascend910.cce PROPERTIES LANGUAGE CXX
)

if(BUILD_OPEN_PROJECT)
    set(CCE_DIR ${ASCEND_CANN_PACKAGE_PATH}/tools/ccec_compiler/bin)
    # dynamic_mul_int8_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_232000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_int8_232000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_232000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_223000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_int8_223000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_223000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_int8_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_int8_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_mul_int8_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_232000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_223000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_210010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_210000000.o 
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int8_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_232000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_223000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_210010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int8_210000000.o
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int8_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_minimum_int8_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_232000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/minimum_Ascend910_int8_232000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_232000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_223000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/minimum_Ascend910_int8_223000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_223000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/minimum_Ascend910_int8_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/minimum_Ascend910_int8_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_minimum_int8_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_232000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_223000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_210010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_210000000.o 
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int8_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_232000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_223000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_210010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/minimum_Ascend910_int8_210000000.o
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int8_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_maximum_int8_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_232000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/maximum_Ascend910_int8_232000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_232000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_223000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/maximum_Ascend910_int8_223000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_223000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/maximum_Ascend910_int8_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/maximum_Ascend910_int8_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_maximum_int8_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_232000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_223000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_210010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_210000000.o 
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int8_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_232000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_223000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_210010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/maximum_Ascend910_int8_210000000.o
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int8_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_add_int8_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_232000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_int8_232000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_232000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_223000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_int8_223000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_223000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_int8_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_int8_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_add_int8_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_232000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_223000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_210010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_210000000.o 
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int8_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_232000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_223000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_210010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int8_210000000.o
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int8_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_mul_int64_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_2147483647.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_int64_2147483647.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_2147483647.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_int64_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_int64_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_mul_int64_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_2147483647.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_210010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_210000000.o
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int64_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_2147483647.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_210010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_int64_210000000.o
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int64_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_minimum_int64_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_2147483647.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/min_Ascend910_int64_2147483647.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_2147483647.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/min_Ascend910_int64_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/min_Ascend910_int64_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_minimum_int64_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_2147483647.o
        ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_210010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_210000000.o
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int64_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_2147483647.o
                ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_210010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/min_Ascend910_int64_210000000.o
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int64_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_maximum_int64_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_2147483647.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/max_Ascend910_int64_2147483647.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_2147483647.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/max_Ascend910_int64_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/max_Ascend910_int64_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_maximum_int64_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_2147483647.o
        ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_210010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_210000000.o
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int64_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_2147483647.o
                ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_210010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/max_Ascend910_int64_210000000.o
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int64_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_add_int64_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_2147483647.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_int64_2147483647.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_2147483647.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_int64_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_int64_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_add_int64_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_2147483647.o
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_210010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_210000000.o
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int64_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_2147483647.o
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_210010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_int64_210000000.o
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int64_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_mul_float32_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_232010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float32_232010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_232010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_232000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float32_232000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_232000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_223010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float32_223010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_223010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_223000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float32_223000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_223000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float32_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float32_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_mul_float32_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_232010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_232000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_223010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_223000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_210010000.o 
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_210000000.o 
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_float32_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_232010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_232000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_223010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_223000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_210010000.o 
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float32_210000000.o 
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_float32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_mul_float16_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_232010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float16_232010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_232010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_232000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float16_232000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_232000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_223010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float16_223010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_223010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_223000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float16_223000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_223000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float16_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/mul_Ascend910_float16_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_mul_float16_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_232010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_232000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_223010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_223000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_210010000.o 
        ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_210000000.o 
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_float16_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_232010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_232000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_223010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_223000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_210010000.o 
                ${CMAKE_CURRENT_BINARY_DIR}/mul_Ascend910_float16_210000000.o 
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_float16_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_add_float16_v80
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_232010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_float16_232010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_232010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_232000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_float16_232000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_232000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_223010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_float16_223010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_223010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_223000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_float16_223000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_223000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_210010000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_float16_210010000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_210010000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_210000000.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/add_Ascend910_float16_210000000.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_210000000.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(dynamic_add_float16_v80 ALL
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 -q -x 
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_232010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_232000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_223010000.o
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_223000000.o
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_210010000.o 
        ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_210000000.o 
        -static -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_float16_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_232010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_232000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_223010000.o
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_223000000.o
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_210010000.o 
                ${CMAKE_CURRENT_BINARY_DIR}/add_Ascend910_float16_210000000.o 
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_float16_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_minimum_float32_v80 
    add_custom_target(dynamic_minimum_float32_v80 ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float32_v80.o)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float32_v80.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_minimum_float32_v80.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float32_v80.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_maximum_float32_v80
    add_custom_target(dynamic_maximum_float32_v80 ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float32_v80.o)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float32_v80.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_maximum_float32_v80.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float32_v80.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    
    # dynamic_minimum_float16_v80
    add_custom_target(dynamic_minimum_float16_v80 ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float16_v80.o)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float16_v80.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_minimum_float16_v80.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float16_v80.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float16_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_maximum_float16_v80
    add_custom_target(dynamic_maximum_float16_v80 ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float16_v80.o)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float16_v80.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_maximum_float16_v80.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float16_v80.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float16_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_mul_int32_v80
    add_custom_target(dynamic_mul_int32_v80 ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int32_v80.o)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int32_v80.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_mul_int32_v80.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int32_v80.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_minimum_int32_v80
    add_custom_target(dynamic_minimum_int32_v80 ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int32_v80.o)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int32_v80.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_minimum_int32_v80.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int32_v80.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_maximum_int32_v80
    add_custom_target(dynamic_maximum_int32_v80 ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int32_v80.o)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int32_v80.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_maximum_int32_v80.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int32_v80.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # dynamic_add_int32_v80
    add_custom_target(dynamic_add_int32_v80 ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int32_v80.o)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int32_v80.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_add_int32_v80.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int32_v80.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )

    # MemSet_dynamic_AtomicAddrClean_1_ascend910
    add_custom_target(MemSet_dynamic_AtomicAddrClean_1_ascend910 ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/MemSet_dynamic_AtomicAddrClean_1_ascend910.o)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/MemSet_dynamic_AtomicAddrClean_1_ascend910.o
            COMMAND ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/MemSet_dynamic_AtomicAddrClean_1_ascend910.cce
            -o ${CMAKE_CURRENT_BINARY_DIR}/MemSet_dynamic_AtomicAddrClean_1_ascend910.o --cce-aicore-arch=dav-c100
            --cce-aicore-only -mllvm -cce-aicore-mask-opt=false -fstack-protector-all
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/MemSet_dynamic_AtomicAddrClean_1_ascend910.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
else()
    get_filename_component(CCE_DIR "${CMAKE_LINKER}" PATH)
    
    # dynamic_mul_int8_v80
    add_library(dynamic_mul_int8_v80_obj
        OBJECT  mul_Ascend910_int8_232000000.cce
                mul_Ascend910_int8_223000000.cce
                mul_Ascend910_int8_210010000.cce
                mul_Ascend910_int8_210000000.cce
    )
    
    target_compile_options(dynamic_mul_int8_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_mul_int8_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_mul_int8_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int8_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_mul_int8_v80 dynamic_mul_int8_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int8_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_minimum_int8_v80
    add_library(dynamic_minimum_int8_v80_obj
        OBJECT  minimum_Ascend910_int8_232000000.cce
                minimum_Ascend910_int8_223000000.cce
                minimum_Ascend910_int8_210010000.cce
                minimum_Ascend910_int8_210000000.cce
    )
    
    target_compile_options(dynamic_minimum_int8_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_minimum_int8_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_minimum_int8_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int8_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_minimum_int8_v80 dynamic_minimum_int8_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int8_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_maximum_int8_v80
    add_library(dynamic_maximum_int8_v80_obj
        OBJECT  maximum_Ascend910_int8_232000000.cce
                maximum_Ascend910_int8_223000000.cce
                maximum_Ascend910_int8_210010000.cce
                maximum_Ascend910_int8_210000000.cce
    )
    
    target_compile_options(dynamic_maximum_int8_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_maximum_int8_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_maximum_int8_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int8_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_maximum_int8_v80 dynamic_maximum_int8_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int8_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_add_int8_v80
    add_library(dynamic_add_int8_v80_obj
        OBJECT  add_Ascend910_int8_232000000.cce
                add_Ascend910_int8_223000000.cce
                add_Ascend910_int8_210010000.cce
                add_Ascend910_int8_210000000.cce
    )
    
    target_compile_options(dynamic_add_int8_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_add_int8_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_add_int8_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int8_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_add_int8_v80 dynamic_add_int8_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int8_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_mul_int64_v80
    add_library(dynamic_mul_int64_v80_obj
        OBJECT  mul_Ascend910_int64_2147483647.cce
                mul_Ascend910_int64_210010000.cce
                mul_Ascend910_int64_210000000.cce
    )
    
    target_compile_options(dynamic_mul_int64_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_mul_int64_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_mul_int64_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int64_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_mul_int64_v80 dynamic_mul_int64_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int64_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_minimum_int64_v80
    add_library(dynamic_minimum_int64_v80_obj
        OBJECT  min_Ascend910_int64_2147483647.cce
                min_Ascend910_int64_210010000.cce
                min_Ascend910_int64_210000000.cce
    )
    
    target_compile_options(dynamic_minimum_int64_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_minimum_int64_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_minimum_int64_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int64_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_minimum_int64_v80 dynamic_minimum_int64_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int64_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_maximum_int64_v80
    add_library(dynamic_maximum_int64_v80_obj
        OBJECT  max_Ascend910_int64_2147483647.cce
                max_Ascend910_int64_210010000.cce
                max_Ascend910_int64_210000000.cce
    )
    
    target_compile_options(dynamic_maximum_int64_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_maximum_int64_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_maximum_int64_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int64_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_maximum_int64_v80 dynamic_maximum_int64_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int64_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_add_int64_v80
    add_library(dynamic_add_int64_v80_obj
        OBJECT  add_Ascend910_int64_2147483647.cce
                add_Ascend910_int64_210010000.cce
                add_Ascend910_int64_210000000.cce
    )
    
    target_compile_options(dynamic_add_int64_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_add_int64_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_add_int64_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int64_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_add_int64_v80 dynamic_add_int64_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int64_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_mul_float32_v80
    add_library(dynamic_mul_float32_v80_obj
        OBJECT  mul_Ascend910_float32_232010000.cce
                mul_Ascend910_float32_232000000.cce
                mul_Ascend910_float32_223010000.cce
                mul_Ascend910_float32_223000000.cce
                mul_Ascend910_float32_210010000.cce
                mul_Ascend910_float32_210000000.cce
    )
    
    target_compile_options(dynamic_mul_float32_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_mul_float32_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_mul_float32_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_float32_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_mul_float32_v80 dynamic_mul_float32_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_float32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_minimum_float32_v80
    add_library(dynamic_minimum_float32_v80_obj
        OBJECT  dynamic_minimum_float32_v80.cce
    )
    
    target_compile_options(dynamic_minimum_float32_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_minimum_float32_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_minimum_float32_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float32_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_minimum_float32_v80 dynamic_minimum_float32_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_maximum_float32_v80
    add_library(dynamic_maximum_float32_v80_obj
        OBJECT  dynamic_maximum_float32_v80.cce
    )
    
    target_compile_options(dynamic_maximum_float32_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_maximum_float32_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_maximum_float32_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float32_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_maximum_float32_v80 dynamic_maximum_float32_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_mul_float16_v80
    add_library(dynamic_mul_float16_v80_obj
        OBJECT  mul_Ascend910_float16_232010000.cce
                mul_Ascend910_float16_232000000.cce
                mul_Ascend910_float16_223010000.cce
                mul_Ascend910_float16_223000000.cce
                mul_Ascend910_float16_210010000.cce
                mul_Ascend910_float16_210000000.cce
    )
    
    target_compile_options(dynamic_mul_float16_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_mul_float16_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_mul_float16_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_float16_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_mul_float16_v80 dynamic_mul_float16_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_float16_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_add_float16_v80
    add_library(dynamic_add_float16_v80_obj
        OBJECT  add_Ascend910_float16_232010000.cce
                add_Ascend910_float16_232000000.cce
                add_Ascend910_float16_223010000.cce
                add_Ascend910_float16_223000000.cce
                add_Ascend910_float16_210010000.cce
                add_Ascend910_float16_210000000.cce
    )
    
    target_compile_options(dynamic_add_float16_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_add_float16_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_add_float16_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_float16_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_add_float16_v80 dynamic_add_float16_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_float16_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_minimum_float16_v80
    add_library(dynamic_minimum_float16_v80_obj
        OBJECT  dynamic_minimum_float16_v80.cce
    )
    
    target_compile_options(dynamic_minimum_float16_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_minimum_float16_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_minimum_float16_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float16_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_minimum_float16_v80 dynamic_minimum_float16_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_float16_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_maximum_float16_v80
    add_library(dynamic_maximum_float16_v80_obj
        OBJECT  dynamic_maximum_float16_v80.cce
    )
    
    target_compile_options(dynamic_maximum_float16_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_maximum_float16_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_maximum_float16_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float16_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_maximum_float16_v80 dynamic_maximum_float16_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_float16_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_mul_int32_v80
    add_library(dynamic_mul_int32_v80_obj
        OBJECT  dynamic_mul_int32_v80.cce
    )
    
    target_compile_options(dynamic_mul_int32_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_mul_int32_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_mul_int32_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int32_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_mul_int32_v80 dynamic_mul_int32_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_mul_int32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_minimum_int32_v80
    add_library(dynamic_minimum_int32_v80_obj
        OBJECT  dynamic_minimum_int32_v80.cce
    )
    
    target_compile_options(dynamic_minimum_int32_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_minimum_int32_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_minimum_int32_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int32_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_minimum_int32_v80 dynamic_minimum_int32_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_minimum_int32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_maximum_int32_v80
    add_library(dynamic_maximum_int32_v80_obj
        OBJECT  dynamic_maximum_int32_v80.cce
    )
    
    target_compile_options(dynamic_maximum_int32_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_maximum_int32_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_maximum_int32_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int32_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_maximum_int32_v80 dynamic_maximum_int32_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_maximum_int32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # dynamic_add_int32_v80
    add_library(dynamic_add_int32_v80_obj
        OBJECT  dynamic_add_int32_v80.cce
    )
    
    target_compile_options(dynamic_add_int32_v80_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(dynamic_add_int32_v80
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:dynamic_add_int32_v80_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int32_v80.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(dynamic_add_int32_v80 dynamic_add_int32_v80_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dynamic_add_int32_v80.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
    # MemSet_dynamic_AtomicAddrClean_1_ascend910
    add_library(MemSet_dynamic_AtomicAddrClean_1_ascend910_obj
        OBJECT  MemSet_dynamic_AtomicAddrClean_1_ascend910.cce
    )
    
    target_compile_options(MemSet_dynamic_AtomicAddrClean_1_ascend910_obj PRIVATE
        -c
        -O2
        --cce-aicore-arch=dav-c100
        --cce-aicore-only
        -Wl,z,now
        -fPIE -pie
        -z now
        -fstack-protector-all
        -mllvm -cce-aicore-mask-opt=false
    )
    
    add_custom_target(MemSet_dynamic_AtomicAddrClean_1_ascend910
        COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext 0 $<TARGET_OBJECTS:MemSet_dynamic_AtomicAddrClean_1_ascend910_obj> -o ${CMAKE_CURRENT_BINARY_DIR}/MemSet_dynamic_AtomicAddrClean_1_ascend910.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(MemSet_dynamic_AtomicAddrClean_1_ascend910 MemSet_dynamic_AtomicAddrClean_1_ascend910_obj)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/MemSet_dynamic_AtomicAddrClean_1_ascend910.o OPTIONAL
        DESTINATION ${INSTALL_LIBRARY_DIR}
    )
endif()