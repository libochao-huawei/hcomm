/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef UNSAFE_TABLE_H
#define UNSAFE_TABLE_H

#include "unsafe/unsafe_hash_table.h"

namespace Hss {

constexpr u64 DEFAULT_BUCKETS_NUM = 8;
template <typename K, typename V, typename KT = KeyTraits<K>, typename VT = ValueTraits<V>>
class UnsafeTable {
public:
    using USHashTable = UnsafeHashtable<K, V, KT, VT>;
    using Iterator = typename USHashTable::HCoreIter;
    using ElePtr = typename USHashTable::ElePtr;
    using InsertResult = typename USHashTable::InsertResult;

    explicit UnsafeTable(const u64 &bucketsNum = DEFAULT_BUCKETS_NUM) : m_usHashTable(bucketsNum)
    {
    }

    ~UnsafeTable ()
    {
    }

    inline s32 Insert(const K &key, const V &value)
    {
        K tmpKey = key;

        return this->m_usHashTable.Insert(tmpKey, value);
    }

    inline ElePtr Find(const K &key)
    {
        K tmpKey = key;

        return this->m_usHashTable.Find(tmpKey);
    }

    inline std::pair<ElePtr, InsertResult> InsertOrFind(const K &key)
    {
        K tmpKey = key;

        return this->m_usHashTable.InsertOrFind(tmpKey);
    }

    inline u64 Size() const
    {
        return this->m_usHashTable.GetEleNum();
    }

    inline s32 Reserve(u64 tableSize)
    {
        return this->m_usHashTable.Reserve(tableSize);
    }

    inline s32 Clear()
    {
        return this->m_usHashTable.ClearTable();
    }

    inline Iterator Begin()
    {
        return this->m_usHashTable.Begin();
    }

    inline Iterator End()
    {
        return this->m_usHashTable.End();
    }

private:
    USHashTable m_usHashTable{};
};

}
#endif