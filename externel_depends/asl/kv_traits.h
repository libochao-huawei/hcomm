/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef KV_TRAITS_H
#define KV_TRAITS_H

#include <atomic>
#include <memory>
#include "hss_types.h"

namespace Hss {

const u32 TEMP_VALUE_LEN = 4;

template <class T>
struct KeyTraits {
    static constexpr T NULL_KEY = static_cast<T>(-1);

    inline static u64 Hash(const T &key)
    {
        return static_cast<u64>(key);
    }
};

template <class T>
struct ValueTraits {
    inline static bool IsValueNull(T &value)
    {
        if (value == nullptr) {
            return true;
        }
        return false;
    }

    inline static T GetNullValue()
    {
        return T();
    }
};

struct HashValue {
public:
    HashValue()
    {
    }

    HashValue(HashValue &val) = delete;
    HashValue(const HashValue &val) = delete;
    HashValue& operator=(HashValue &val) = delete;
    HashValue& operator=(const HashValue &val) = delete;

    HashValue(HashValue &&val)
    {
        uniqValue = std::move(val.uniqValue);
    }

    HashValue& operator=(HashValue &&val)
    {
        if (this == &val) {
            return *this;
        }
        uniqValue = std::move(val.uniqValue);
        return *this;
    }

    bool operator==(std::nullptr_t) const
    {
        if (uniqValue == nullptr) {
            return true;
        }
        return false;
    }

    void ReleaseResource()
    {
        uniqValue = nullptr;
    }

    std::unique_ptr<u8[]> uniqValue{ nullptr };
};

struct KTraits {
    static constexpr int64_t NULL_KEY = -1;

    inline static u32 Hash(const int64_t &key)
    {
        return static_cast<u32>(key);
    }
};

struct VTraits {
    inline static bool IsValueNull(const HashValue &value)
    {
        if (value.uniqValue.get() == nullptr) {
            return true;
        }
        return false;
    }

    inline static HashValue GetNullValue()
    {
        return HashValue();
    }
};

struct FilterHashValue {
public:
    FilterHashValue()
    {
    }

    FilterHashValue(FilterHashValue &val) = delete;
    FilterHashValue(const FilterHashValue &val) = delete;
    FilterHashValue& operator=(FilterHashValue &val) = delete;
    FilterHashValue& operator=(const FilterHashValue &val) = delete;

    explicit FilterHashValue(u32 num)
    {
        counter.store(num, std::memory_order_relaxed);
    }

    FilterHashValue(u32 num, std::unique_ptr<u8[]> &val)
    {
        counter.store(num, std::memory_order_relaxed);
        uniqValue = std::move(val);
        this->SetFlag();
    }

    FilterHashValue(FilterHashValue &&val)
    {
        counter.store(val.counter.load(std::memory_order_relaxed), std::memory_order_relaxed);
        uniqValue = std::move(val.uniqValue);
    }

    FilterHashValue& operator=(FilterHashValue &&val)
    {
        if (this == &val) {
            return *this;
        }
        counter.store(val.counter.load(std::memory_order_relaxed), std::memory_order_relaxed);
        uniqValue = std::move(val.uniqValue);
        return *this;
    }

    bool operator==(std::nullptr_t) const
    {
        if (uniqValue == nullptr) {
            return true;
        }
        return false;
    }

    void SetFlag()
    {
        flag.store(true, std::memory_order_relaxed);
    }

    bool GetFlag() const
    {
        return flag.load(std::memory_order_relaxed);
    }

    void ReleaseResource()
    {
        uniqValue = nullptr;
    }

    std::unique_ptr<u8[]> uniqValue{ nullptr };
    std::atomic<u32> counter{ 0 };
    std::atomic<bool> flag{ false };
};

struct FHashValue {
public:
    FHashValue()
    {
    }

    FHashValue(FHashValue &val) = delete;
    FHashValue(const FHashValue &val) = delete;
    FHashValue& operator=(FHashValue &val) = delete;
    FHashValue& operator=(const FHashValue &val) = delete;

    FHashValue(FHashValue &&val)
    {
        fHash = std::move(val.fHash);
    }

    FHashValue& operator=(FHashValue &&val)
    {
        if (this == &val) {
            return *this;
        }
        fHash = std::move(val.fHash);
        return *this;
    }

    bool operator==(std::nullptr_t) const
    {
        if (fHash == nullptr) {
            return true;
        }
        return false;
    }

    std::unique_ptr<FilterHashValue> fHash{ nullptr };
};

struct FVTraits {
    inline static bool IsValueNull(const FHashValue &value)
    {
        if (value.fHash == nullptr) {
            return true;
        }

        return false;
    }

    inline static FHashValue GetNullValue()
    {
        return FHashValue();
    }
};

}

#endif
