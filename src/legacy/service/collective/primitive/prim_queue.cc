/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 主stream队列
 * Author: zhangqingwei
 * Create: 2024-02-04
 */
#include "prim_queue.h"
#include "iostream"

namespace Hccl {

void PrimQueue::Append(unique_ptr<Primitive> prim)
{
    auto primPtr = prim.get();
    if (primPtr->GetType() == PrimType::POST_TO) {
        PrimPostTo &postTo = static_cast<PrimPostTo &>(*primPtr);
        postTo.SetParent(shared_from_this());
    } else if (primPtr->GetType() == PrimType::WAIT_FROM) {
        PrimWaitFrom &waitFrom = static_cast<PrimWaitFrom &>(*primPtr);
        waitFrom.SetParent(shared_from_this());
    } else if (primPtr->GetType() == PrimType::WAIT_GROUP) {
        PrimWaitGroup &waitFrom = static_cast<PrimWaitGroup &>(*primPtr);
        waitFrom.SetParent(shared_from_this());
    }
    HierarchicalQueue::Append(std::move(prim));
}
void PrintPrimQueue(const PrimQueue &queue)
{
    HCCL_INFO("Master queue: ");
    for (auto it = queue.Iter(); it.HasNext(); ++it) {
        HCCL_INFO("    %s", it->Describe().c_str());
    }
    for (auto slaveIt = queue.IterSlaves(); slaveIt.HasNext(); ++slaveIt) {
        HCCL_INFO("Slave queue %u: ", slaveIt->GetId());
        for (auto it = slaveIt->Iter(); it.HasNext(); ++it) {
            HCCL_INFO("    %s", it->Describe().c_str());
        }
    }
}
} // namespace Hccl