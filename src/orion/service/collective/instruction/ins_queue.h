/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: instruction queue header file
 * Author: wenxuemin
 * Create: 2024-02-04
 */

#ifndef HCCLV2_INS_QUEUE_H
#define HCCLV2_INS_QUEUE_H

#include "instruction.h"
#include "hierarchical_queue.h"

#include <unordered_set>
#include <vector>
namespace Hccl {
using namespace std;

class InsQueue : public HierarchicalQueue<Instruction, InsQueue>, public enable_shared_from_this<InsQueue> {
public:
    vector<LinkData> GetUniqueLinks()
    {
        unordered_set<LinkData> uniqueLinks;
        for (auto iter = Iter(); iter.HasNext(); ++iter) {
            auto linkPtr = iter->GetLink();
            if (linkPtr == nullptr) {
                    continue;
                }
            uniqueLinks.insert(*linkPtr);
        }
        for (auto slaveIter = IterSlaves(); slaveIter.HasNext(); ++slaveIter) {
            for (auto iterSlave = slaveIter->Iter(); iterSlave.HasNext(); ++iterSlave) {
                auto linkPtrSlave = iterSlave->GetLink();
                if (linkPtrSlave == nullptr) {
                    continue;
                }
                uniqueLinks.insert(*linkPtrSlave);
            }
        }
        return {uniqueLinks.begin(), uniqueLinks.end()};
    };

    void Append(unique_ptr<Instruction> ins) override
    {
        auto insPtr = ins.get();
        if (insPtr->GetType() == InstructionType::LOCAL_POST_TO) {
            InsLocalPostTo &postTo = static_cast<InsLocalPostTo &>(*insPtr);
            postTo.SetPostQid(GetId());
        } else if (insPtr->GetType() == InstructionType::LOCAL_WAIT_FROM) {
            InsLocalWaitFrom &waitFrom = static_cast<InsLocalWaitFrom &>(*insPtr);
            waitFrom.SetWaitQid(GetId());
        } else if (insPtr->GetType() == InstructionType::LOCAL_WAIT_GROUP) {
            InsLocalWaitGroup &waitGroup = static_cast<InsLocalWaitGroup &>(*insPtr);
            waitGroup.SetWaitQid(GetId());
        } else if (insPtr->GetType() == InstructionType::LOCAL_BCAST_POST) {
            InsLocalBcastPost &bcastPost = static_cast<InsLocalBcastPost &>(*insPtr);
            bcastPost.SetPostQid(GetId());
        }
        HierarchicalQueue::Append(std::move(ins));
    }
};
} // namespace Hccl

#endif // !HCCLV2_INS_QUEUE_H
