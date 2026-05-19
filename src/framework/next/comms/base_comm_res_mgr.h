/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMM_RES_MGR_H
#define BASE_COMM_RES_MGR_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "hccl_types.h"
#include "hccl_common.h"

namespace hcomm {

class BaseCommRes;

class BaseCommResMgr {
public:
    static BaseCommResMgr& Instance();

    BaseCommRes* GetOrCreate(uint32_t devPhyId);
    BaseCommRes* GetBaseCommRes(uint32_t devPhyId);
    HcclResult DestroyBaseCommRes(uint32_t devPhyId);
    HcclResult DestroyAll();
    uint32_t GetDeviceCount() const;

    void VisitBaseCommRes(const std::function<void(BaseCommRes*)>& visitor);

private:
    BaseCommResMgr();
    ~BaseCommResMgr();

    BaseCommResMgr(const BaseCommResMgr&) = delete;
    BaseCommResMgr& operator=(const BaseCommResMgr&) = delete;

private:
    std::unordered_map<uint32_t, std::unique_ptr<BaseCommRes>> baseCommResMap_;
    mutable std::mutex mutex_;
};

} // namespace hcomm

#endif // BASE_COMM_RES_MGR_H