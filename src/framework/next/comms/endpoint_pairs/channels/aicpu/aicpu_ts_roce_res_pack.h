/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCOMM_AICPU_TS_ROCE_RES_PACK_H
#define HCOMM_AICPU_TS_ROCE_RES_PACK_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace hcomm {
namespace roce_res_pack {

// 与 Hccl::AicpuResMgrType 中 STREAM 之前条目数量一致（不含 __COUNT__）
static constexpr std::size_t kAicpuResMgrSlotCount = 9U;

// 与 Hccl::MODULE_NAME_LEN 一致
static constexpr std::size_t kModuleNameLen = 128U;

struct ModuleData {
    char name[kModuleNameLen]{};
    std::vector<char> data{};
};

inline void AppendRaw(std::vector<char> &out, const void *p, std::size_t n)
{
    const auto *b = static_cast<const char *>(p);
    out.insert(out.end(), b, b + n);
}

template <typename T>
inline void AppendPod(std::vector<char> &out, const T &v)
{
    AppendRaw(out, &v, sizeof(T));
}

// 对齐 Hccl::BinaryStream 对 std::vector<char> 的序列化：先 size_t，再逐字节 char
inline void AppendCharVec(std::vector<char> &out, const std::vector<char> &v)
{
    const std::size_t sz = v.size();
    AppendPod(out, sz);
    out.insert(out.end(), v.begin(), v.end());
}

// 对齐 Hccl::operator<<(BinaryStream &, const ModuleData &)
inline void AppendModuleData(std::vector<char> &out, const ModuleData &m)
{
    AppendRaw(out, m.name, kModuleNameLen);
    AppendCharVec(out, m.data);
}

// 对齐 Hccl::operator<<(BinaryStream &, const std::vector<ModuleData> &)
inline void AppendModuleDataVec(std::vector<char> &out, const std::vector<ModuleData> &vec)
{
    const std::size_t n = vec.size();
    AppendPod(out, n);
    for (const auto &m : vec) {
        AppendModuleData(out, m);
    }
}

inline std::vector<char> PackAicpuResBlob(std::vector<ModuleData> &slots)
{
    std::vector<char> out;
    AppendModuleDataVec(out, slots);
    return out;
}

inline bool SetModuleName(ModuleData &m, const char *name)
{
    std::fill_n(m.name, kModuleNameLen, '\0');
    if (name == nullptr) {
        return true;
    }
    const std::size_t len = std::strlen(name);
    if (len >= kModuleNameLen) {
        return false;
    }
    std::memcpy(m.name, name, len);
    return true;
}

} // namespace roce_res_pack
} // namespace hcomm

#endif // HCOMM_AICPU_TS_ROCE_RES_PACK_H
