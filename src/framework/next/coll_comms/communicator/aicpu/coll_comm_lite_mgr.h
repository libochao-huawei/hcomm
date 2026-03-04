/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef COLL_COMM_LITE_MGR_H
#define COLL_COMM_LITE_MGR_H

#include <memory>
#include <mutex>
#include <vector>
#include "coll_comm_aicpu.h"

namespace hccl {
/**
 * @note 职责：实现多个集合通信通信域上下文的创建、销毁管理，及多通信域资源、信息的共享等。
 */
class CollCommLiteMgr {
private:
    CollCommLiteMgr();
    ~CollCommLiteMgr();

public:
    static CollCommLiteMgr *GetInstance();
    void RegisteCollComm(CollCommAicpu* collComm);
    void UnRegisteCollComm(CollCommAicpu* collComm);
    std::unordered_map<std::string, CollCommAicpu*> GetAllCollComms();

private:
    static CollCommLiteMgr* instance_;
    std::unordered_map<std::string, CollCommAicpu*> allCollCommLites_;
};
}
#endif // COLL_COMM_LITE_MGR_H