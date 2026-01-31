/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: one sided data
 * Create: 2025-12-27
 */

#ifndef LOCAL_RDMA_RMA_BUFFER_MANAGER_H
#define LOCAL_RDMA_RMA_BUFFER_MANAGER_H

#include "../buffer/local_rdma_rma_buffer.h"
#include "../../pub_inc/rma_buffer_mgr.h"

namespace Hccl {

using LocalRdmaRmaBufferMgr = RmaBufferMgr<BufferKey<uintptr_t, u64>, std::shared_ptr<LocalRdmaRmaBuffer>>;

class LocalRdmaRmaBufferManager {
public:
    static LocalRdmaRmaBufferMgr *GetInstance();
private:
    LocalRdmaRmaBufferManager() = default;
    ~LocalRdmaRmaBufferManager() = default;
    LocalRdmaRmaBufferManager(const LocalRdmaRmaBufferManager &)            = delete;
    LocalRdmaRmaBufferManager &operator=(const LocalRdmaRmaBufferManager &) = delete;
};

}   // namespace Hccl

#endif //LOCAL_RDMA_RMA_BUFFER_MANAGER_H