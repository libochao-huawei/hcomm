/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../pub_inc/trace.h"

namespace Hccl {

Trace::Trace()
{
}

HcclResult Trace::Init(std::string &logInfo)
{
    traceHandle = TraceCreate(logInfo.c_str());//Handle只申请不释放，由atrace组件释放
    return HcclResult::HCCL_SUCCESS;
}

void Trace::Save(std::string &buffer)
{
    //对传入的buffersize进行判断。
    u32 len = DEFAULT_ATRACE_MSG_SIZE - 1;
    u32 pos = 0;
    u32 totLen = 0;
    u32 submitLen = 0;
    totLen = buffer.length();
    bool ret = true;
    if (traceHandle == TRACE_INVALID_HANDLE) {
        HCCL_WARNING("Tracve::Save Info = %s", buffer.c_str());
        return;
    }
    char *startPos = static_cast<char *>(const_cast<char *>(buffer.c_str()));
    while (pos < totLen) {
        submitLen = (pos + len <= totLen) ? len : (totLen - pos);
        ret = ret && TraceSubmit(traceHandle, startPos, submitLen);
        if (!ret) {
            HCCL_WARNING("trace submit failed, ret[%d], traceInfo = [%s]", ret, startPos);
        }
        startPos += submitLen;
        pos += submitLen;
    }
    if (!ret) {
        HCCL_WARNING("trace submit failed, ret[%d], traceInfo = [%s]", ret, startPos);
    }
}

Trace::~Trace()
{
    TraceDestroy(traceHandle);
}

} // namespace Hccl
