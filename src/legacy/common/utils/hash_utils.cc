/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: hash utils
 * Author: limengjiao
 * Create: 2023-10-28
 */

#include "hash_utils.h"

namespace Hccl {

std::size_t HashCombine(std::initializer_list<std::size_t> hashItem)
{
    std::size_t res     = 17;
    std::size_t padding = 31;
    for (auto begin = hashItem.begin(); begin != hashItem.end(); ++begin) {
        res = padding * res + (*begin);
    }
    return res;
}

} // namespace Hccl