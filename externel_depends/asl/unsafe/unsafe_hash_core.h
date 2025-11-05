/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef UNSAFE_HASH_CORE_H
#define UNSAFE_HASH_CORE_H

#include <map>
#include <algorithm>
#include "hash_method.h"
#include "hss_types.h"
#include "hdds_common.h"
#include "kv_traits.h"
#include "hss_log.h"

namespace Hss {

template <typename K, typename V, typename KT = KeyTraits<K>, typename VT = ValueTraits<V>>
class UnsafeHashCore {
    using HC = UnsafeHashCore<K, V, KT, VT>;
    using Mapiter = typename std::map<K, V>::iterator;
public:
    static constexpr f32 LOAD_RATE = 0.75;
    static constexpr f32 MAP_NUM_THRESHOLD = 0.001 * LOAD_RATE;
    static constexpr u64 MIN_CELLS_SIZE = 8;
    static constexpr u64 SIZE_MASK_SUB = 1;

    static constexpr u8 REHASHED = 0x2;
    static constexpr u8 MAP_EXIST = 0x80;
    static constexpr u8 CLEAN_HIGH = 0X0f;
    static constexpr u64 SEARCH_LIMIT = 128;
    static constexpr u64 MAP_NUM_APPEND = 1;

    struct Cell {
        K key{ KT::NULL_KEY };
        V value{ VT::GetNullValue() };
    };

    struct Delta {
        u8 delta1{ 0 };
        u8 delta2{ 0 };
    };

    // 上面两个结构体构成hashTable基本元素，通过结构体数组构造hashTable。Entry数量可能极大，需要严格考虑内存对齐
    struct HashCell {
        u8 flag{ 0 };
        struct Delta deltas{};
        struct Cell cell{};
        std::unique_ptr<std::map<K, V>> map{ nullptr };
    };

    // 建议元素值和类型尽量保持不变，可能对外部有影响
    enum class InsertResult : s32 {
        NO_MEM = HDDS_E_MEM,
        ERROR = HDDS_FAIL,
        FOUND = 0,
        NEW_INSERTED = 1,
        OVER_FLOW = 2,
    };

    // 当前对迭代器需求功能单一，当前直接用V *替换
    using ElePtr = V *;

    const u64 m_linearSearchLimit{ SEARCH_LIMIT };
    u64 m_cellsSize{ 0 };
    u64 m_mapNum{ 0 };
    u64 m_eleNum{ 0 };
    std::unique_ptr<UnsafeHashCore<K, V, KT, VT>> m_nextHashCore{};
    std::vector<std::pair<HashCell*, u64>> m_validIndex{};
    std::vector<bool> m_validMapIndex{};

    class iterator {
        using Mapiter = typename std::map<K, V>::iterator;
    public:

        iterator()
        {
        }

        iterator(const iterator &iter) = default;

        ~iterator()
        {
            m_value = nullptr;
            m_hashCells = nullptr;
        }

        inline iterator& operator=(const iterator &iter) = default;

        inline bool operator!=(const iterator &iter) const
        {
            if (this->m_endFlag && iter.m_endFlag) {
                return false;
            }

            if (this->m_inMap) {
                if (this->m_mapIt == iter.m_mapIt) {
                    return false;
                }
            } else {
                if (this->m_index == iter.m_index) {
                    return false;
                }
            }

            return true;
        }

        inline bool operator==(const iterator &iter) const
        {
            if (this->m_endFlag && iter.m_endFlag) {
                return true;
            }

            if (this->m_inMap) {
                if (this->m_mapIt == iter.m_mapIt) {
                    return true;
                }
            } else {
                if (this->m_index == iter.m_index) {
                    return true;
                }
            }

            return false;
        }

        inline iterator& operator++(int)
        {
            if (this->m_inMap) {
                IterInMap(*this);
                return *this;
            }

            this->m_index++;
            if (m_index >= m_validIter->size()) {
                if (this->m_inNullKey) {
                    m_endFlag = true;
                    return *this;
                }

                if (m_nullKey != nullptr && m_valueForNullKey != nullptr) {
                    m_inNullKey = true;
                    m_key = m_nullKey;
                    m_value = m_valueForNullKey;
                }

                return *this;
            }

            this->m_hashCells = (*m_validIter)[m_index].first;
            this->m_inMap = (*m_validMap)[(*m_validIter)[m_index].second];
            FindNextIter(*this, this->m_inMap);
            return *this;
        }

        inline const K* GetKey() const
        {
            return m_key;
        }

        inline void SetKey(const K *tmp)
        {
            m_key = tmp;
        }

        inline V* GetValue() const
        {
            return m_value;
        }

        inline void SetValue(V *tmp)
        {
            m_value = tmp;
        }

        inline void SetInMap(bool tmp)
        {
            m_inMap = tmp;
        }

        inline std::map<K, V>* GetMapPtr() const
        {
            return m_mapPtr;
        }

        inline void SetMapPtr(std::map<K, V> *tmp)
        {
            m_mapPtr = tmp;
        }

        inline Mapiter GetMapIt() const
        {
            return m_mapIt;
        }

        inline void SetMapIt(Mapiter tmp)
        {
            m_mapIt = tmp;
        }

        inline void SetIndex(u64 tmp)
        {
            m_index = tmp;
        }

        inline void SetHashCell(HashCell *tmp)
        {
            m_hashCells = tmp;
        }

        inline void SetCellSizeMask(u64 tmp)
        {
            m_cellsSizeMask = tmp;
        }

        inline void SetValidIter(std::vector<std::pair<HashCell*, u64>> *tmp)
        {
            m_validIter = tmp;
        }

        inline void SetValidMap(std::vector<bool> *tmp)
        {
            m_validMap = tmp;
        }

        inline void SetNullKey(const K *tmp)
        {
            m_nullKey = tmp;
        }

        inline void SetValueForNullKey(V *tmp)
        {
            m_valueForNullKey = tmp;
        }

        inline void SetEndFlag()
        {
            m_endFlag = true;
        }

    private:
        bool m_endFlag{ false };
        bool m_inNullKey{ false };
        const K *m_nullKey{ nullptr };
        V *m_valueForNullKey{ nullptr };
        HashCell *m_hashCells{ nullptr };
        std::map<K, V> *m_mapPtr{ nullptr };
        Mapiter m_mapIt{};
        const K *m_key{ nullptr };
        V *m_value{ nullptr };
        std::vector<std::pair<HashCell*, u64>> *m_validIter{};
        std::vector<bool> *m_validMap{};
        u64 m_index{ 0 };
        u64 m_cellsSizeMask{ 0 };
        bool m_inMap{ false };

        inline void SetKeyValue(K* key, V *valuePtr)
        {
            this->m_key = key;
            this->m_value = valuePtr;
        }

        inline void IterInMap(iterator &it)
        {
            it.m_mapIt++;
            if (it.m_mapIt != it.m_mapPtr->end()) {
                it.m_key = &(it.m_mapIt->first);
                it.m_value = &(it.m_mapIt->second);
            } else {
                it.m_inMap = false;
                it.m_mapPtr = nullptr;
                it.m_key = &(it.m_hashCells->cell.key);
                it.m_value = &(it.m_hashCells->cell.value);
            }
        }

        inline void FindNextIter(iterator &it, bool inMap)
        {
            if (inMap) {
                it.m_mapPtr = it.m_hashCells->map.get();
                it.m_mapIt = it.m_mapPtr->begin();
                it.m_key = &(it.m_mapIt->first);
                it.m_value = &(it.m_mapIt->second);
            } else {
                it.m_key = &(it.m_hashCells->cell.key);
                it.m_value = &(it.m_hashCells->cell.value);
            }
        }
    };

    explicit UnsafeHashCore(const u64 &cellsSize = MIN_CELLS_SIZE) : m_validMapIndex(cellsSize, false)
    {
        this->m_cellsSize = cellsSize;
        m_validIndex.reserve(this->m_cellsSize);

        MAKE_UNIQUE_EXECEPTION_CATCH((this->m_hashCells =
            std::make_unique<struct HashCell[]>(this->m_cellsSize)), throw std::bad_alloc());
        this->m_cellsSizeMask = this->m_cellsSize - SIZE_MASK_SUB;
    }

    ~UnsafeHashCore()
    {
    }

    inline iterator Begin()
    {
        iterator tmpIt;
        if (m_eleNum == 0) {
            return tmpIt;
        }

        HashCell &firstHashCell = *(m_validIndex[0].first);
        tmpIt.SetIndex(0);
        if (firstHashCell.map == nullptr) {
            tmpIt.SetKey(&(firstHashCell.cell.key));
            tmpIt.SetValue(&(firstHashCell.cell.value));
            tmpIt.SetValidIter(&m_validIndex);
            tmpIt.SetValidMap(&m_validMapIndex);
        } else {
            tmpIt.SetMapPtr(firstHashCell.map.get());
            tmpIt.SetMapIt(tmpIt.GetMapPtr()->begin());
            tmpIt.SetKey(&(tmpIt.GetMapIt()->first));
            tmpIt.SetValue(&(tmpIt.GetMapIt()->second));
            tmpIt.SetInMap(true);
            tmpIt.SetValidIter(&m_validIndex);
            tmpIt.SetValidMap(&m_validMapIndex);
        }

        tmpIt.SetHashCell(&firstHashCell);
        tmpIt.SetCellSizeMask(m_cellsSizeMask);

        if (m_flagForNullKey) {
            tmpIt.SetNullKey(&m_nullKey);
            tmpIt.SetValueForNullKey(&m_valueForNullKey);
        }

        return tmpIt;
    }

    inline iterator End()
    {
        iterator tmpIt;
        tmpIt.SetIndex(m_eleNum);
        tmpIt.SetEndFlag();
        return tmpIt;
    }

    inline bool ThresholdCheck() const
    {
        if ((m_mapNum + MAP_NUM_APPEND) > static_cast<u32>(m_cellsSize * MAP_NUM_THRESHOLD)) {
            return true;
        }

        return false;
    }

    inline u64 GetHashCode(const K &key) const
    {
        return KT::Hash(key) & this->m_cellsSizeMask;
    }

    inline ElePtr Find(const K &key)
    {
        ElePtr value = this->FindValueProcess(key);
        return value;
    }

    // 增加返回当前hashCore指针（this）
    inline InsertResult InsertOrFind(const K &key, ElePtr &outIt, HC *&aimHashCore)
    {
        InsertResult result = this->InsertOrFindProcess(key, outIt, aimHashCore);
        return result;
    }

    inline UnsafeHashCore<K, V, KT, VT> *GetNextHashCore() const
    {
        return this->m_nextHashCore.get();
    }

    inline HashCell *GetFirstHashCell() const
    {
        return this->m_hashCells.get();
    }

    inline void SetMapExist(u8 &flag) const
    {
        u8 newFlag = (flag & CLEAN_HIGH) | MAP_EXIST;
        flag = newFlag;
    }

    inline s32 AddNewCore(std::unique_ptr<UnsafeHashCore<K, V, KT, VT>> &newHashCore)
    {
        this->m_nextHashCore = std::move(newHashCore);
        return HDDS_SUCCESS;
    }

    inline s32 ClearCore()
    {
        u64 validNum = m_validIndex.size();
        HashCell *tmpCell;
        for (u64 i = 0; i < validNum; ++i) {
            tmpCell = m_validIndex[i].first;
            tmpCell->cell.key = KT::NULL_KEY;
            tmpCell->cell.value = VT::GetNullValue();
            tmpCell->flag = 0;
            tmpCell->deltas.delta1 = 0;
            tmpCell->deltas.delta2 = 0;
            if (m_validMapIndex[m_validIndex[i].second]) {
                tmpCell->map.reset();
                m_validMapIndex[m_validIndex[i].second] = false;
            }
            m_validIndex.clear();
        }

        m_flagForNullKey = false;
        m_valueForNullKey = VT::GetNullValue();
        m_eleNum = 0;
        return HDDS_SUCCESS;
    }

    inline u64 GetCellSize() const
    {
        return this->m_cellsSize;
    }

    inline u64 GetSizeMask() const
    {
        return this->m_cellsSizeMask;
    }

    inline u64 GetLinearSearchLimit() const
    {
        return this->m_linearSearchLimit;
    }

    inline InsertResult NewInserted()
    {
        m_eleNum++;
        return InsertResult::NEW_INSERTED;
    }

    inline void AppendMapNum()
    {
        m_mapNum++;
    }

    inline u64 GetEleNum() const
    {
        return this->m_eleNum;
    }

    inline void SetEleNum(u64 eleNum)
    {
        this->m_eleNum = eleNum;
    }

    K m_nullKey { KT::NULL_KEY };
    bool m_flagForNullKey { false };
    V m_valueForNullKey { VT::GetNullValue() };
private:
    std::unique_ptr<struct HashCell[]> m_hashCells;
    u64 m_cellsSizeMask{};

    inline InsertResult MapFindOrInsert(const K &key, const u64 &bucketHead, ElePtr &outIt)
    {
        HashCell &hashCell = m_hashCells[bucketHead];
        std::map<K, V> *dataMap = hashCell.map.get();
        auto it = dataMap->find(key);
        // 没找到，需要插入新元素到map
        if (it == dataMap->end()) {
            auto emplaceResult = dataMap->emplace(key, VT::GetNullValue());
            outIt = &(emplaceResult.first->second);
            return NewInserted();
        }

        outIt = &(it->second);
        return InsertResult::FOUND;
    }

    inline ElePtr MapFind(const K &key, const u64 &bucketHead) const
    {
        HashCell &hashCell = m_hashCells[bucketHead];
        std::map<K, V> *dataMap = hashCell.map.get();
        auto it = dataMap->find(key);
        // 没找到，需要插入新元素到map
        if (it == dataMap->end()) {
            HSS_ERR("key is not exist in map! bucketHead[%llu], tableSize[%llu]", bucketHead, this->m_cellsSize);
            return nullptr;
        }

        return &(it->second);
    }

    inline InsertResult LinearAndMap(const K &key, const u64 &bucketHead, ElePtr &outIt)
    {
        HashCell &hashCell = m_hashCells[bucketHead];

        std::map<K, V> *newMap;
        NEW_NOTHROW(newMap, (std::map<K, V>), return InsertResult::ERROR);
        // 只将当前的key value插入map
        auto emplaceResult = newMap->emplace(key, VT::GetNullValue());
        outIt = &(emplaceResult.first->second);
        hashCell.map.reset(newMap);
        AppendMapNum();
        SetMapExist(hashCell.flag);
        // 修改map标识符
        m_validMapIndex[bucketHead] = true;
        return NewInserted();
    }

    // 为了Null Key使用的流程
    inline InsertResult InsertOrFindProcessForNullKey(ElePtr &outIt)
    {
        if (m_flagForNullKey) {
            outIt = &m_valueForNullKey;
            return InsertResult::FOUND;
        }

        m_flagForNullKey = true;
        outIt = &m_valueForNullKey;
        return NewInserted();
    }

    // 插入专用，返回查到的Cell的指针或者新插入的Cell的指针以及插入结果
    inline InsertResult InsertOrFindProcess(const K &key, ElePtr &outIt, HC *&aimHashCore)
    {
        aimHashCore = this;
        if (UNLIKELY(key == KT::NULL_KEY)) {
            return InsertOrFindProcessForNullKey(outIt);
        }

        u64 bucketIndex = GetHashCode(key);
        const u64 bucketHead = bucketIndex;
        HashCell &hashCell = m_hashCells[bucketIndex];

        K currentKey = hashCell.cell.key;
        if (currentKey == key) {
            outIt = &hashCell.cell.value;
            return InsertResult::FOUND;
        } else if (currentKey == KT::NULL_KEY) {
            // 添加阈值判断
            hashCell.cell.key = key;
            outIt = &hashCell.cell.value;
            m_validIndex.emplace_back(std::make_pair(&hashCell, bucketIndex));
            return NewInserted();
        }

        u8 delta = hashCell.deltas.delta1;
        u8 *deltaPtr = &(hashCell.deltas.delta1);
        while (delta != 0) {
            bucketIndex += delta;
            u64 currentIndex = bucketIndex & this->m_cellsSizeMask;
            HashCell &hashCell2 = m_hashCells[currentIndex];
            if (hashCell2.cell.key == key) {
                outIt = &hashCell2.cell.value;
                return InsertResult::FOUND;
            }

            delta = hashCell2.deltas.delta2;
            deltaPtr = &(hashCell2.deltas.delta2);
        }

        if ((hashCell.flag & MAP_EXIST) != 0) {
            return MapFindOrInsert(key, bucketHead, outIt);
        }

        return LinearSearch(key, bucketHead, outIt, bucketIndex, deltaPtr);
    }

    inline InsertResult LinearSearch(const K &key, const u64 &bucketHead, ElePtr &outIt, u64 &bucketIndex,
        u8 *&deltaPtr)
    {
        K currentKey;
        const u64 lastIndex = bucketIndex;
        u64 linearSearchRemaining = std::min(m_cellsSizeMask, m_linearSearchLimit);
        while (linearSearchRemaining-- > 0) {
            bucketIndex++;
            u64 currentIndex = bucketIndex & this->m_cellsSizeMask;
            HashCell &hashCell = m_hashCells[currentIndex];
            currentKey = hashCell.cell.key;
            if (currentKey == KT::NULL_KEY) {
                hashCell.cell.key = key;
                *deltaPtr = bucketIndex - lastIndex;
                outIt = &hashCell.cell.value;
                m_validIndex.emplace_back(std::make_pair(&hashCell, currentIndex));
                return NewInserted();
            }
        }

        if (ThresholdCheck()) {
            return InsertResult::OVER_FLOW;
        }

        return LinearAndMap(key, bucketHead, outIt);
    }

    // 为了Null Key使用的流程
    inline V *FindValueProcessForNullKey()
    {
        return m_flagForNullKey ? &m_valueForNullKey : nullptr;
    }

    // 查询专用,返回查到的Cell的指针
    inline V *FindValueProcess(const K &key)
    {
        if (UNLIKELY(key == KT::NULL_KEY)) {
            return FindValueProcessForNullKey();
        }

        u64 bucketIndex = GetHashCode(key);
        const u64 bucketHead = bucketIndex;
        HashCell &hashCell = m_hashCells[bucketHead];
        struct Cell &currentCell = hashCell.cell;
        K &currentKey = currentCell.key;
        if (currentKey == key) {
            return &(currentCell.value);
        } else if (currentKey == KT::NULL_KEY) {
            return nullptr;
        }

        u8 delta = hashCell.deltas.delta1;
        while (delta != 0) {
            bucketIndex += delta;
            u64 currentIndex = bucketIndex & this->m_cellsSizeMask;
            HashCell &hashCell2 = m_hashCells[currentIndex];
            struct Cell &currentCell2 = hashCell2.cell;
            if (currentCell2.key == key) {
                return &(currentCell2.value);
            }

            delta = hashCell2.deltas.delta2;
        }

        if ((hashCell.flag & MAP_EXIST) != 0) {
            return MapFind(key, bucketHead);
        }

        // 走完流程，没有找到
        return nullptr;
    }
};
}

#endif
