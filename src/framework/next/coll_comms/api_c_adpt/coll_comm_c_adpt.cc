/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <memory>
#include "coll_comm_c_adpt.h"
#include "../coll_comm_mgr.h"

using namespace hccl;
static std::unique_ptr<CollCommMgr> g_collCommMgr = std::make_unique<CollCommMgr>();

/**
 * @note 职责：集合通信的通信域管理的C接口的C到C++适配
 */

/**
 * @note C接口适配参考示例
 * @code {.c}
 * HcclResult HcclCommDestroy(HcclComm comm) {
 *     return g_collCommMgr->DestroyComm(comm);
 * }
 * @endcode
 */