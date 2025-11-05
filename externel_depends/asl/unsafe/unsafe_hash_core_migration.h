/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef UNSAFE_HASH_CORE_MIGRATION_H
#define UNSAFE_HASH_CORE_MIGRATION_H

#include "hss_types.h"
#include "hdds_common.h"
#include "unsafe_hash_core.h"

namespace Hss {

template <typename HC, typename K, typename V, typename KT = KeyTraits<K>, typename VT = ValueTraits<V>>
class UnsafeHashCoreMigration {
public:
    static constexpr u32 CREATE_TABLE_FACTOR = 2;

    using HCell = typename HC::HashCell;
    using HCP = HCell *;
    using Cell = typename HC::Cell;
    using Delta = typename HC::Delta;
    using Iterator = typename HC::iterator;
    using AHCP = HC *;

    UnsafeHashCoreMigration()
    {
    }

    ~UnsafeHashCoreMigration()
    {
    }

    inline void MoveEntryForNullKeyToNewTable(HC &src, HC &dest) const
    {
        dest.m_flagForNullKey = src.m_flagForNullKey;
        dest.m_valueForNullKey = src.m_valueForNullKey;
    }

    inline s32 ReserveOldEntryToNewTable(HC &src, HC &dest) const
    {
        MoveEntryForNullKeyToNewTable(src, dest);
        u64 validSize = src.m_validIndex.size();
        for (u64 i = 0; i < validSize; i++) {
            s32 ret = RehashInsert(src, dest, src.m_validIndex[i].second);
            if (ret < 0) {
                HSS_ERR("RehashInsert failed! ret[%d]", ret);
                return ret;
            }
        }

        return HDDS_SUCCESS;
    }

    inline s32 MoveOldEntryToNewTable(HC &src, HC &dest)
    {
        u64 srcTableSize = src.GetCellSize();
        HCP srcEntry = src.GetFirstHashCell();
        m_destSizeMask = dest.GetSizeMask();
        m_linearSearchLimit = dest.GetLinearSearchLimit();

        MoveEntryForNullKeyToNewTable(src, dest);
        for (u64 i = 0; i < srcTableSize; i++) {
            HCell &aimHashCell = srcEntry[i];
            const K &currentKey = aimHashCell.cell.key;
            if (currentKey == KT::NULL_KEY) {
                continue;
            }

            s32 ret = RehashInsert(src, dest, i);
            if (ret < 0) {
                HSS_ERR("RehashInsert failed! ret[%d]", ret);
                return ret;
            }
        }

        return HDDS_SUCCESS;
    }

    inline s32 SetNextHashCore(std::unique_ptr<HC> &newHashCore) const
    {
        HC *core = *m_firstHashCore;
        if (core == nullptr) {
            HSS_ERR("m_curHashCore is nullptr");
            return HDDS_FAIL;
        }

        core->AddNewCore(newHashCore);
        return HDDS_SUCCESS;
    }

    inline s32 MakeNewTable(HC &tableElement, HC *&aimTable) const
    {
        u64 newTableSize = tableElement.GetCellSize() * CREATE_TABLE_FACTOR;

        std::unique_ptr<HC> newHashCore;
        MAKE_UNIQUE_EXECEPTION_CATCH((newHashCore = std::make_unique<HC>(newTableSize)), return HDDS_E_MEM);
        s32 ret = SetNextHashCore(newHashCore);
        if (ret < 0) {
            HSS_ERR("SetNextHashCore failed!");
            return ret;
        }

        aimTable = tableElement.GetNextHashCore();
        aimTable->SetEleNum(tableElement.GetEleNum());

        return ret;
    }

    inline s32 DeleteOldTable() const
    {
        HC *core = *m_firstHashCore;
        if (core == nullptr) {
            HSS_ERR("m_curHashCore is nullptr");
            return HDDS_FAIL;
        }

        (*m_firstHashCore) = core->GetNextHashCore();
        if (m_firstHashCore == nullptr) {
            HSS_ERR("m_firstHashCore is nullptr");
            return HDDS_FAIL;
        }

        return HDDS_SUCCESS;
    }

    s32 BeginWholeTableMigration(AHCP *&firstHC, HC *tableElement, HC *&aimTable)
    {
        m_firstHashCore = firstHC;
        s32 ret = MakeNewTable(*tableElement, aimTable);
        if (ret < 0) {
            HSS_ERR("MakeNewTable failed! ret[%d]", ret);
            return ret;
        }

        ret = MoveOldEntryToNewTable(*tableElement, *aimTable);
        if (ret < 0) {
            HSS_ERR("MoveOldEntryToNewTable failed!, ret[%d]", ret);
            return ret;
        }

        ret = DeleteOldTable();
        if (ret < 0) {
            HSS_ERR("DeleteOldTable failed! ret[%d]", ret);
            return ret;
        }

        return HDDS_SUCCESS;
    }

    inline s32 RehashInsert(HC &src, HC &dest, const u64 &index) const
    {
        HCP srcEntry = src.GetFirstHashCell();
        HCell &srcHashCell = srcEntry[index];
        u8 flag = srcHashCell.flag;

        // 如果有map则把map中的元素全部搬移到下一张表
        if ((flag & HC::MAP_EXIST) != 0) {
            std::map<K, V> *dataMap = srcHashCell.map.get();
            for (auto it = dataMap->begin(); it != dataMap->end(); it++) {
                s32 ret = RehashInsertEntry(dest, it->first, it->second);
                if (ret != HDDS_SUCCESS) {
                    HSS_ERR("RehashInsertEntry failed! src[%llu], dst[%llu]", src.GetCellSize(), dest.GetCellSize());
                    return ret;
                }
            }
        }

        s32 ret = RehashInsertEntry(dest, srcHashCell.cell.key, srcHashCell.cell.value);
        if (ret != HDDS_SUCCESS) {
            HSS_ERR("RehashInsertEntry failed! src[%llu], dst[%llu]", src.GetCellSize(), dest.GetCellSize());
            return ret;
        }

        return ret;
    }

    inline s32 RehashInsertEntry(HC &dest, const K &key, V &valuePtr) const
    {
        u64 bucketIndex = dest.GetHashCode(key);
        HCP dstEntry = dest.GetFirstHashCell();
        u64 currentIndex = bucketIndex;
        HCell &currentIndexHashCell = dstEntry[currentIndex];
        Cell &currentCell = currentIndexHashCell.cell;

        K currentKey = currentCell.key;
        if (currentKey == KT::NULL_KEY) {
            currentCell.key = key;
            currentCell.value = valuePtr;
            dest.m_validIndex.emplace_back(std::make_pair(&dstEntry[currentIndex], currentIndex));
            return HDDS_SUCCESS;
        }

        if ((currentIndexHashCell.flag & HC::MAP_EXIST) != 0) {
            std::map<K, V> *dataMap = currentIndexHashCell.map.get();
            dataMap->emplace(key, valuePtr);
            return HDDS_SUCCESS;
        }

        u8 delta = currentIndexHashCell.deltas.delta1;
        u8 *deltaPtr = &currentIndexHashCell.deltas.delta1;
        while (delta != 0) {
            currentIndex += delta;
            u64 currIndex = currentIndex & dest.GetSizeMask();
            deltaPtr = &dstEntry[currIndex].deltas.delta2;
            delta = *deltaPtr;
        }

        u64 lastIndex = currentIndex;
        u64 linearSearchRemaining = std::min(m_destSizeMask, m_linearSearchLimit);
        while (linearSearchRemaining-- > 0) {
            currentIndex++;
            u64 currIndex = currentIndex & dest.GetSizeMask();
            Cell &currentCell3 = dstEntry[currIndex].cell;
            currentKey = currentCell3.key;
            if (currentKey == KT::NULL_KEY) {
                currentCell3.key = key;
                currentCell3.value = valuePtr;
                *deltaPtr = currentIndex - lastIndex;
                dest.m_validIndex.emplace_back(std::make_pair(&dstEntry[currIndex], currIndex));
                return HDDS_SUCCESS;
            }
        }

        return RehashMapInsert(dest, key, valuePtr, dstEntry);
    }

    inline s32 RehashMapInsert(HC &dest, const K &key, V &valuePtr, HCP &dstEntry) const
    {
        const u64 &bucketIndex = dest.GetHashCode(key);
        HCell &bucketIndexHashCell = dstEntry[bucketIndex];
        u8 &flag = bucketIndexHashCell.flag;

        std::map<K, V> *newMap;
        NEW_NOTHROW(newMap, (std::map<K, V>), return HDDS_FAIL);
        newMap->emplace(key, valuePtr);
        bucketIndexHashCell.map.reset(newMap);
        dest.AppendMapNum();
        dest.SetMapExist(flag);
        dest.m_validMapIndex[bucketIndex] = true;
        return HDDS_SUCCESS;
    }

private:
    AHCP *m_firstHashCore{ nullptr };
    u64 m_destSizeMask{};
    u64 m_linearSearchLimit{};
};
}

#endif
