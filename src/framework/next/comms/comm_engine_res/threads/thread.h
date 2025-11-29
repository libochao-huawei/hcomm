/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef THREAD_H
#define THREAD_H

#include <vector>
#include <memory>

namespace hcomm {
/**
 * @note 职责：通信引擎的Thread的C++抽象接口类，表达并行资源，内部包含thread间的同步Notify。
 */
class Thread {
public:
    explicit Thread(CommEngine engineType);
    virtual ~Thread() = default;

    // 获取Notify数量
    virtual uint32_t GetNotifyNum() const = 0;

protected:
    ThreadHandle handle_{};
    CommEngine engineType_{};
    std::vector<Notify> notifys_{};
};

#endif // THREAD_H
