/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef READ_WRITE_LOCK_BASE_H
#define READ_WRITE_LOCK_BASE_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <iomanip>
#include <climits>
#include "log.h"

class ReadWriteLockBase {
public:
    ReadWriteLockBase() : readers_(0), writers_(0), writing_(false) {}
    void readLock();
    void readUnlock();
    void writeLock();
    void writeUnlock();

private:
    std::mutex mutex_;
    std::condition_variable readerCV_;
    std::condition_variable writerCV_;
    int readers_;  // num of readers
    int writers_;  // num of waiting readers
    bool writing_;
};

#endif  // READ_WRITE_LOCK_BASE_H
