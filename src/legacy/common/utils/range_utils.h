//
// Created by w00422550 on 1/16/24.
// Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
//

#ifndef HCCLV2_RANGE_UTIL_H
#define HCCLV2_RANGE_UTIL_H
#include <limits>

namespace Hccl {
/**
 * 判断以begin1为起点，长度为len1的范围是否包含以begin2为起点，长度为len2的范围
 * @tparam T1
 * @tparam T2
 * @param begin1  范围1的起点
 * @param len1    范围1的长度
 * @param begin2  范围2的起点
 * @param len2    范围2的长度
 * @return 范围1包含范围2则返回true；否则返回false
 */
template <typename T1, typename T2> inline bool IsRangeInclude(T1 begin1, T2 len1, T1 begin2, T2 len2)
{
    // 检查begin1 + len1是否会溢出
    if (len1 > static_cast<T2>(std::numeric_limits<T1>::max() - begin1)) {
        return false;
    }
    // 检查begin2 + len2是否会溢出
    if (len2 > static_cast<T2>(std::numeric_limits<T1>::max() - begin2)) {
        return false;
    }
    return (begin1 <= begin2 && begin1 + len1 >= begin2 + len2);
}
} // namespace Hccl

#endif // HCCLV2_RANGE_UTIL_H
