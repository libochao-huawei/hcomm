/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "read_write_lock_base.h"

void ReadWriteLockBase::readLock()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (readers_ == INT_MAX) {
        HCCL_ERROR("[%s] readers[%d] exceed max lock num", __func__, readers_);
        return;
    }
    while (writers_ > 0 || writing_) {
        readerCV_.wait(lock);
    }
    readers_++;
}

void ReadWriteLockBase::readUnlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (readers_ == 0) {
        HCCL_RUN_WARNING("[%s] readers[%d], do not need to unlock", __func__, readers_);
        return;
    }
    readers_--;
    if (readers_ == 0) {
        writerCV_.notify_one();
    }
}

void ReadWriteLockBase::writeLock() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (writers_ == INT_MAX) {
        HCCL_ERROR("[%s] writers_[%d] exceed max lock waiting num", __func__, writers_);
        return;
    }
    writers_++;
    while (readers_ > 0 || writing_) {
        writerCV_.wait(lock);
    }
    writing_ = true;
    writers_--;
}

void ReadWriteLockBase::writeUnlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    writing_ = false;
    // notify all waiting readers and writers
    readerCV_.notify_all();
    writerCV_.notify_one();
}
