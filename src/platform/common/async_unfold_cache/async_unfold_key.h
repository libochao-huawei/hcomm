/*
 * This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __ASYNC_UNFOLD_KEY_H__
#define __ASYNC_UNFOLD_KEY_H__

#include <cstdint>
#include <functional>
#include <string>

#include "hccl_types.h"

namespace hccl {

// 注意: 与OpUnfoldKey不同, AsyncUnfoldKey作为AICPU异步展开算子的标识符, 以算子实例 (即aipcu kernel) 为粒度
// 而OpUnfoldKey作为AICPU同步展开算子的标识符, 以算子类型为粒度
struct AsyncUnfoldKey {
    explicit AsyncUnfoldKey();
    explicit AsyncUnfoldKey(const AsyncUnfoldKey& other); // 拷贝构造函数 (make_pair需要)

    HcclResult Init();

    // 用于debug
    std::string GetKeyString() const;

    bool operator==(const AsyncUnfoldKey& other) const; // 重载operator==用于std::unordered_map中相等比较
    const AsyncUnfoldKey& operator=(const AsyncUnfoldKey& other); // 拷贝赋值操作符

    // TODOSSY: key info
};

}; // namespace hccl

namespace std {

// 全特化std::hash<AsyncUnfoldKey>, 用于std::unordered_map中计算哈希值
template<>
struct hash<hccl::AsyncUnfoldKey> {
    size_t operator()(const hccl::AsyncUnfoldKey& key) const {
        // TODOSSY: 使用std::hash计算key中每个字段的哈希值

        // TODOSSY: 简单的哈希混合
        size_t hashValue = 0;

        return hashValue;
    }
};

} // namespace std

#endif // __ASYNC_UNFOLD_KEY_H__