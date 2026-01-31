/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: LocalRdmaRmaBufferManager
 * Create: 2025-12-27
 */

#include "local_rdma_rma_buffer_manager.h"

namespace Hccl {

using namespace std;

LocalRdmaRmaBufferMgr *LocalRdmaRmaBufferManager::GetInstance()
{
    static LocalRdmaRmaBufferMgr instance;
    return &instance;
}

} // namespace Hccl