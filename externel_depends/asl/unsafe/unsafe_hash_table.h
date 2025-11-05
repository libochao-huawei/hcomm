/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef UNSAFE_HASH_TABLE_H
#define UNSAFE_HASH_TABLE_H

#include "hss_log.h"
#include "hss_types.h"
#include "hdds_common.h"
#include "unsafe_hash_core.h"
#include "unsafe_hash_core_migration.h"

namespace Hss {

template <typename K, typename V, typename KT, typename VT = ValueTraits<V>>
class UnsafeHashtable {
public:
    using HCore = UnsafeHashCore<K, V, KT, VT>;
    using HCell = typename HCore::HashCell;
    using HCoreIter = typename HCore::iterator;
    using HCM = UnsafeHashCoreMigration<HCore, K, V, KT, VT>;
    using ElePtr = typename HCore::ElePtr;
    using InsertResult = typename HCore::InsertResult;

    static const u64 LOAD_RATE_NUMER = 3;
    static const u64 LOAD_RATE_DENOM = 4;
    static const u64 MIN_HASH_TABLE_SIZE = 8;
    static const u32 MORE_LIKELY_IN_THIS = 1;

    explicit UnsafeHashtable(const u64 &tableSize = MIN_HASH_TABLE_SIZE)
    {
        this->m_tableSize = tableSize < MIN_HASH_TABLE_SIZE ? MIN_HASH_TABLE_SIZE : tableSize;
        this->m_tableSize = this->m_tableSize / LOAD_RATE_NUMER * LOAD_RATE_DENOM;
        this->m_tableSize = RoundUpPowerOf2(this->m_tableSize);

        MAKE_UNIQUE_EXECEPTION_CATCH((this->m_headOfHashCores =
            std::make_unique<HCore>(this->m_tableSize)), throw std::bad_alloc());

        this->m_curHashCore = this->m_headOfHashCores.get();
    }

    ~UnsafeHashtable()
    {
    }

    inline ElePtr Find(const K &key)
    {
        HCore *hashCore = this->m_curHashCore;
        return hashCore->Find(key);
    }

    std::pair<ElePtr, InsertResult> InsertOrFind(const K &key)
    {
        HCore *hashCore = this->m_curHashCore;
        ElePtr outIt = nullptr;
        HCore *aimHashCore{};
        InsertResult result = hashCore->InsertOrFind(key, outIt, aimHashCore);
        if (LIKELY(result == InsertResult::NEW_INSERTED) || LIKELY(result == InsertResult::FOUND)) {
            return { outIt, result };
        } else if (result == InsertResult::OVER_FLOW) {
            HCore *aimTable = nullptr;
            s32 ret = MigrateHashCore(*aimHashCore, aimTable);
            if (UNLIKELY(ret == HDDS_E_MEM)) {
                HSS_ERR("no enough mem!");
                return { outIt, InsertResult::NO_MEM };
            } else if (UNLIKELY(ret != HDDS_SUCCESS)) {
                HSS_ERR("MigrateHashCore failed!");
                return { outIt, InsertResult::ERROR };
            }

            result = aimTable->InsertOrFind(key, outIt, aimHashCore);
        }

        if (result == InsertResult::ERROR || result == InsertResult::NO_MEM) {
            HSS_ERR("InsertOrFind failed!");
            outIt = nullptr;
        }

        return { outIt, result };
    }

    inline s32 Insert(const K &key, const V &value)
    {
        std::pair<ElePtr, InsertResult> result = this->InsertOrFind(key);
        // 新插入key
        if (result.second == InsertResult::NEW_INSERTED) {
            this->AssignValue(result.first, value);
            return HDDS_SUCCESS;
        } else if (result.second == InsertResult::FOUND) {
            HSS_ERR("key exist!");
            return HDDS_E_PARA;
        } else {
            HSS_ERR("insert failed!");
            return static_cast<s32>(result.second);
        }
    }

    inline HCore *GetHeadHashCore() const
    {
        return this->m_curHashCore;
    }

    s32 MigrateHashCore(HCore &srcCore, HCore *&aimTable)
    {
        HCore **firstHashCore = &m_curHashCore;
        s32 ret = this->m_migration.BeginWholeTableMigration(firstHashCore, &srcCore, aimTable);
        m_headOfHashCores = std::move(m_headOfHashCores->m_nextHashCore);
        return ret;
    }

    inline u64 GetEleNum() const
    {
        return m_curHashCore->GetEleNum();
    }

    inline s32 ClearTable()
    {
        return m_curHashCore->ClearCore();
    }

    inline HCoreIter Begin()
    {
        return m_curHashCore->Begin();
    }

    inline HCoreIter End()
    {
        return m_curHashCore->End();
    }

    inline s32 Reserve(u64 tableSize)
    {
        u64 tmpSize = tableSize / LOAD_RATE_NUMER * LOAD_RATE_DENOM;
        tmpSize = RoundUpPowerOf2(tmpSize);
        if (tmpSize > this->m_tableSize) {
            this->m_tableSize = tmpSize;
            std::unique_ptr<HCore> newHeadOfHashCores;
            MAKE_UNIQUE_EXECEPTION_CATCH((newHeadOfHashCores =
                std::make_unique<HCore>(this->m_tableSize)), return HDDS_E_MEM);
            this->m_migration.ReserveOldEntryToNewTable(*(this->m_headOfHashCores), *(newHeadOfHashCores));
            this->m_headOfHashCores = std::move(newHeadOfHashCores);
            this->m_curHashCore = this->m_headOfHashCores.get();
        }

        return HDDS_SUCCESS;
    }

private:
    HCM m_migration{};
    HCore *m_curHashCore{};
    std::unique_ptr<HCore> m_headOfHashCores{};
    u64 m_tableSize{};

    inline void AssignValue(ElePtr &iter, const V &value) const
    {
        *iter = value;
    }
};
}

#endif
