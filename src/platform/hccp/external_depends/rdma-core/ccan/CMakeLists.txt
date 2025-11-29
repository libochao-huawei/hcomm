# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
publish_internal_headers(ccan
  array_size.h
  build_assert.h
  check_type.h
  compiler.h
  container_of.h
  ilog.h
  list.h
  minmax.h
  str.h
  str_debug.h
  )

set(C_FILES
  ilog.c
  list.c
  str.c
  )
add_library(ccan STATIC ${C_FILES})
add_library(ccan_pic STATIC ${C_FILES})
set_property(TARGET ccan_pic PROPERTY POSITION_INDEPENDENT_CODE TRUE)
