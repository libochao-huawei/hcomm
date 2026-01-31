//
// Created by w00422550 on 1/16/24.
// Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
//

#ifndef HCCLV2_STL_UTIL_H
#define HCCLV2_STL_UTIL_H

namespace Hccl {
template <typename CONTAINER, typename E> bool Contain(CONTAINER &c, E e)
{
    return c.find(e) != c.end();
}
} // namespace Hccl

#endif // HCCLV2_STL_UTIL_H
