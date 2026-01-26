/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: hash utils header file
 * Author: limengjiao
 * Create: 2023-10-28
 */

#ifndef HASH_UTILS_H
#define HASH_UTILS_H

#include <initializer_list>

namespace Hccl {
std::size_t HashCombine(std::initializer_list<std::size_t> hashItem);

}

#endif // HASH_UTILS_H
