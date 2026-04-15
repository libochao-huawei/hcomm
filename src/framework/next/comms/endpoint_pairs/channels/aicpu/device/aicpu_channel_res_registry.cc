/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_channel_res_handler.h"
#include <unordered_map>
#include "aicpu_ts_roce_res_handler.h"

namespace {
constexpr uint32_t kAicpuTsRoce = 2U;
std::unordered_map<ChannelHandle, uint32_t> g_aicpuChannelResHandleToKind;
} // namespace

void AicpuChannelResRegisterHandleKind(ChannelHandle handle, uint32_t kind)
{
    g_aicpuChannelResHandleToKind[handle] = kind;
}

void AicpuChannelResUnregisterHandleKind(ChannelHandle handle)
{
    g_aicpuChannelResHandleToKind.erase(handle);
}

const std::unordered_map<uint32_t, IAicpuChannelResHandler *> &GetAicpuChannelResHandlers()
{
    static AicpuTsRoceResHandler &roceHandler = AicpuTsRoceResHandler::Instance();
    static const std::unordered_map<uint32_t, IAicpuChannelResHandler *> kHandlers = {
        { kAicpuTsRoce, &roceHandler },
    };
    return kHandlers;
}

bool AicpuChannelResDestroyForHandle(ChannelHandle handle)
{
    auto it = g_aicpuChannelResHandleToKind.find(handle);
    if (it == g_aicpuChannelResHandleToKind.end()) {
        return false;
    }
    const uint32_t kind = it->second;
    const auto &handlers = GetAicpuChannelResHandlers();
    auto hit = handlers.find(kind);
    if (hit == handlers.end()) {
        g_aicpuChannelResHandleToKind.erase(it);
        return false;
    }
    return hit->second->Destroy(handle);
}
