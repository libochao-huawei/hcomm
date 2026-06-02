# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

function(copy_required_file input output)
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR "Input file does not exist: ${input}")
    endif()

    get_filename_component(output_dir "${output}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_dir}")
    configure_file("${input}" "${output}" COPYONLY)
endfunction()

if(DEFINED INPUT AND DEFINED OUTPUT)
    copy_required_file("${INPUT}" "${OUTPUT}")
endif()

if(DEFINED MC2_CLIENT_INPUT AND DEFINED MC2_CLIENT_OUTPUT)
    copy_required_file("${MC2_CLIENT_INPUT}" "${MC2_CLIENT_OUTPUT}")
endif()

if(DEFINED MC2_COMPAT_INPUT AND DEFINED MC2_COMPAT_OUTPUT)
    copy_required_file("${MC2_COMPAT_INPUT}" "${MC2_COMPAT_OUTPUT}")
endif()

if(DEFINED MC2_SERVER_JSON_INPUT AND DEFINED MC2_SERVER_JSON_OUTPUT)
    copy_required_file("${MC2_SERVER_JSON_INPUT}" "${MC2_SERVER_JSON_OUTPUT}")
endif()

if(DEFINED MC2_SERVER_TAR_INPUT AND DEFINED MC2_SERVER_TAR_OUTPUT)
    copy_required_file("${MC2_SERVER_TAR_INPUT}" "${MC2_SERVER_TAR_OUTPUT}")
endif()
