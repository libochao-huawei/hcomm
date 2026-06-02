# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

if(NOT INPUT)
    message(FATAL_ERROR "INPUT is required")
endif()
if(NOT OUTPUT)
    message(FATAL_ERROR "OUTPUT is required")
endif()
if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "Input file does not exist: ${INPUT}")
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
configure_file("${INPUT}" "${OUTPUT}" COPYONLY)
