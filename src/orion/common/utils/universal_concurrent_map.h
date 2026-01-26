/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: universalConcurrentMap
 * Create: 2025-09-16
 */

#ifndef HCCL_UNIVERSAL_CONCURRENT_MAP_H
#define HCCL_UNIVERSAL_CONCURRENT_MAP_H

#include <mutex>
#include <unordered_map>
#include <map>
#include <shared_mutex>

namespace Hccl {

template <typename K, typename V, template <typename...> class M = std::unordered_map, typename... MapArgs>
class UniversalConcurrentMap {
public:
    UniversalConcurrentMap() = default;
    ~UniversalConcurrentMap() = default;

    using MapType = M<K, V, MapArgs...>;
    using Iterator = typename MapType::iterator;
    using ConstIterator = typename MapType::const_iterator;
    using SizeType = typename MapType::size_type;

    // true -> valid
    inline std::pair<Iterator, bool> Find(const K& k)
    {
        std::shared_lock<std::shared_timed_mutex> lock(mapMtx_);
        Iterator it = map_.find(k);
        if (it != map_.end()) {
            return { it, true };
        }

        return { map_.end(), false };
    }

    // true -> valid
    inline std::pair<ConstIterator, bool> Find(const K& k) const
    {
        std::shared_lock<std::shared_timed_mutex> lock(mapMtx_);
        ConstIterator it = map_.find(k);
        if (it != map_.end()) {
            return { it, true };
        }

        return { map_.end(), false };
    }

    // true -> 新插入
    template <class... Args>
    inline std::pair<Iterator, bool> Emplace(Args&&... args)
    {
        std::lock_guard<std::shared_timed_mutex> lock(mapMtx_);

        return map_.emplace(std::forward<Args>(args)...);
    }

    // true -> 新插入，可能抛异常
    template<typename Func, typename... Args>
    inline std::pair<Iterator, bool> EmplaceIfNotExist(const K& k, Func func, Args&&... args)
    {
        std::lock_guard<std::shared_timed_mutex> lock(mapMtx_);
        Iterator it = map_.find(k);
        if (it == map_.end()) {
            return map_.emplace(k, func(std::forward<Args>(args)...));
        }

        return { it, false };
    }

    // 可能抛异常
    template<typename Func, typename... Args>
    inline std::pair<Iterator, bool> EmplaceAndUpdate(const K& k, Func func, Args&&... args)
    {
        std::lock_guard<std::shared_timed_mutex> lock(mapMtx_);

        std::pair<Iterator, bool> it = map_.emplace(k, V());
        func(it.first->second, std::forward<Args>(args)...);

        return it;
    }

    inline V& operator[] (K&& k)
    {
        std::lock_guard<std::shared_timed_mutex> lock(mapMtx_);
        return map_[std::forward<K>(k)];
    }

    inline V& operator[] (const K& k)
    {
        std::lock_guard<std::shared_timed_mutex> lock(mapMtx_);
        return map_[k];
    }

    V& At(const K& k)
    {
        std::lock_guard<std::shared_timed_mutex> lock(mapMtx_);
        return map_.at(k);
    }

    const V& At(const K& k) const
    {
        std::shared_lock<std::shared_timed_mutex> lock(mapMtx_);
        return map_.at(k);
    }

    // 可能抛异常
    template<typename Func, typename... Args>
    inline void EraseAll(Func func, Args&&... args)
    {
        std::lock_guard<std::shared_timed_mutex> lock(mapMtx_);
        for (auto it = map_.begin(); it != map_.end();) {
            func(it->second, std::forward<Args>(args)...);
            it = map_.erase(it);
        }
    }

    inline SizeType Size() const
    {
        std::shared_lock<std::shared_timed_mutex> lock(mapMtx_);
        return map_.size();
    }

    inline void Clear()
    {
        std::lock_guard<std::shared_timed_mutex> lock(mapMtx_);
        map_.clear();
    }

    inline SizeType Erase(const K& k)
    {
        std::lock_guard<std::shared_timed_mutex> lock(mapMtx_);
        return map_.erase(k);
    }

    // 尽量少使用LockFree结尾的函数
    inline SizeType EraseLockFree(const K& k)
    {
        return map_.erase(k);
    }

    inline std::shared_timed_mutex &GetMtx()
    {
        return mapMtx_;
    }

    inline Iterator FindLockFree(const K& k)
    {
        return map_.find(k);
    }

    inline Iterator EndLockFree()
    {
        return map_.end();
    }

    template <class... Args>
    inline std::pair<Iterator, bool> EmplaceLockFree(Args&&... args)
    {
        return map_.emplace(std::forward<Args>(args)...);
    }

private:
    mutable std::shared_timed_mutex mapMtx_{};
    MapType map_{};
};
}

#endif // HCCL_UNIVERSAL_CONCURRENT_MAP_H
