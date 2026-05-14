/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RANK_TABLE_CRC_BRIDGE_H
#define RANK_TABLE_CRC_BRIDGE_H

#include <string>
#include <hccl/hccl_types.h>

namespace hccl {
//桥接函数：记录A5 ranktable文件内容的CRC信息用于一致性校验
HcclResult RecordRankTableCrcV2Bridge(const std::string &rankTableM);
}