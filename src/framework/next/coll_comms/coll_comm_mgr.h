/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef COLL_COMM_MGR_H
#define COLL_COMM_MGR_H

#include <unordered_set>
#include <memory>
#include <mutex>
#include "coll_comm.h"

namespace hccl {
/**
 * @note 职责：实现多个集合通信通信域上下文的创建、销毁管理，及多通信域资源、信息的共享等。
 */
class CollCommMgr {
public:
    CollCommMgr() = default;
    ~CollCommMgr();
    
    // 创建通信域
    HcclResult CreateComm(const char* rankTable, uint32_t rank, const CommConfig& config, HcclComm* comm);
    
    // 销毁通信域
    HcclResult DestroyComm(HcclComm comm);
    
    // 获取通信域
    std::shared_ptr<CollComm> GetComm(HcclComm comm);

private:
    std::unordered_set<std::shared_ptr<CollComm>> collComms_{};
    std::mutex mutex_{};
};
}
#endif // COLL_COMM_MGR_H
