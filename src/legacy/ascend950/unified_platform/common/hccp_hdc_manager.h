/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_HCCP_HDC_MANAGER_H
#define HCCLV2_HCCP_HDC_MANAGER_H
#include <set>
#include <mutex>
#include "orion_adapter_hccp.h"
namespace Hccl {
class HccpHdcManager {
public:
    static HccpHdcManager &GetInstance();
    void                   Init(u32 deviceLogicId);
    HccpHdcManager(const HccpHdcManager &hccpHdcManager)            = delete;
    HccpHdcManager &operator=(const HccpHdcManager &hccpHdcManager) = delete;
    // 测试使用，待修改: 添加编译宏，仅在单元测试时提供此接口
    std::set<u32> GetSet()
    {
        return instances;
    }
    ~HccpHdcManager();

private:
    std::set<u32> instances; // key: deviceLogicId
    std::mutex    managerMutex;
    bool          deinitted{false};
    HccpHdcManager() = default;
    void DestroyAll();
};
} // namespace Hccl
#endif // HCCLV2_HCCP_HDC_MANAGER_H
