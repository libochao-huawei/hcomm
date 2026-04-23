# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set_source_files_properties(
    aiv_broadcast_op.h PROPERTIES LANGUAGE CXX
)

if(CCACHE_PROGRAM)
    set(BISHENG_CCACHE ${CCACHE_PROGRAM})
endif()
if(BUILD_OPEN_PROJECT)
    set(CCE_DIR ${ASCEND_CANN_PACKAGE_PATH}/tools/ccec_compiler/bin)
    add_custom_target(hccl_aiv_broadcast_op ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/hccl_aiv_broadcast_op.o)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/hccl_aiv_broadcast_op.o
            COMMAND ${BISHENG_CCACHE} ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/aiv_broadcast_op.h -o ${CMAKE_CURRENT_BINARY_DIR}/hccl_aiv_broadcast_op_tmp.o 
            -DASCENDC_DUMP=${ASCENDC_DUMP} --cce-aicore-arch=dav-c220-vec
            --cce-aicore-input-parameter-size=28000 --cce-aicore-only -mllvm -cce-aicore-function-stack-size=0x8000 -mllvm -cce-aicore-dcci-insert-for-scalar=false
            -mllvm -cce-aicore-stack-size=0x8000
            -I${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/basic_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/basic_api/interface/
            -I${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/include/adv_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/highlevel_api
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/ascendc/include/basic_api/impl/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/asc/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/asc/impl/basic_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/asc/impl/simt_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/asc/impl/micro_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/include/basic_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/include/simt_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/include/micro_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/include/hccl/
            -I${CMAKE_CURRENT_SOURCE_DIR}/../
            -I${CMAKE_CURRENT_SOURCE_DIR}/../aiv_interface/
            -mllvm -cce-aicore-record-overflow=false -mllvm -cce-aicore-addr-transform -mllvm
            --cce-aicore-jump-expand=true -std=c++17 -fstack-protector-all -DONE_CORE_DUMP_SIZE=1024
            COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext=0 ${CMAKE_CURRENT_BINARY_DIR}/hccl_aiv_broadcast_op_tmp.o -static -o ${CMAKE_CURRENT_BINARY_DIR}/hccl_aiv_broadcast_op.o
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
else()
    get_filename_component(CCE_DIR "${CMAKE_LINKER}" PATH)

    set(TILING_GEN_DIR ${CMAKE_BINARY_DIR}/asc/asc-devkit)

    add_custom_target(hccl_aiv_broadcast_op
            COMMAND ${BISHENG_CCACHE} ${CCE_DIR}/ccec -c -x cce -O2 ${CMAKE_CURRENT_SOURCE_DIR}/aiv_broadcast_op.h -o ${CMAKE_CURRENT_BINARY_DIR}/hccl_aiv_broadcast_op_tmp.o
            -DASCENDC_DUMP=${ASCENDC_DUMP} --cce-aicore-arch=dav-c220-vec
            --cce-aicore-input-parameter-size=28000 --cce-aicore-only -mllvm -cce-aicore-function-stack-size=0x8000 -mllvm -cce-aicore-dcci-insert-for-scalar=false
            -mllvm -cce-aicore-stack-size=0x8000
            -I${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/basic_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/basic_api/interface/
            -I${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/include/adv_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/highlevel_api
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/ascendc/include/basic_api/impl/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/asc/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/asc/impl/basic_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/asc/impl/simt_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/asc/impl/micro_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/include/basic_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/include/simt_api/
            -I${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/include/micro_api/
            -I${TOP_DIR}/hcomm/include/hccl/
            -I${CMAKE_CURRENT_SOURCE_DIR}/../
            -I${CMAKE_CURRENT_SOURCE_DIR}/../aiv_interface/
            -I${TILING_GEN_DIR}
            -mllvm -cce-aicore-record-overflow=false -mllvm -cce-aicore-addr-transform -mllvm
            --cce-aicore-jump-expand=true -std=c++17 -fstack-protector-all -DONE_CORE_DUMP_SIZE=1024
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS kernel_tiling_headers
            COMMAND ${CCE_DIR}/ld.lld -m aicorelinux -Ttext=0 ${CMAKE_CURRENT_BINARY_DIR}/hccl_aiv_broadcast_op_tmp.o -static -o ${CMAKE_CURRENT_BINARY_DIR}/hccl_aiv_broadcast_op.o
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hccl_aiv_broadcast_op.o ${INSTALL_OPTIONAL}
    COMPONENT hcomm
    DESTINATION ${INSTALL_LIBRARY_DIR}
)