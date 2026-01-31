/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description:
 * Author: wenxuemin
 * Create: 2024-02-04
 */
#ifndef HCCLV2_PRIM_QUEUE_H
#define HCCLV2_PRIM_QUEUE_H

#include <memory>
#include "primitive.h"
#include "hierarchical_queue.h"

namespace Hccl {

using namespace std;

class Primitive;

class PrimQueue : public HierarchicalQueue<Primitive, PrimQueue>, public enable_shared_from_this<PrimQueue> {
public:
    void Append(unique_ptr<Primitive> prim) override;
};

void PrintPrimQueue(const PrimQueue &queue);
} // namespace Hccl

#endif
