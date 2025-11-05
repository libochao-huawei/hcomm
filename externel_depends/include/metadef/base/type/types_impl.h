/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_TYPE_TYPES_IMPL_H
#define BASE_TYPE_TYPES_IMPL_H
#include "external/graph/types.h"
#include "external/ge_common/ge_api_types.h"

namespace ge {
class TypeImpl {
 public:
  static const char_t *GetFormatName(Format format);
  static int64_t CeilDiv(const int64_t n1, const int64_t n2);
  static Status CheckInt64MulOverflow(const int64_t a, const int64_t b);
  static int64_t GetSizeInBytes(int64_t element_count, DataType data_type);
};

class PromoteImpl {
 public:
  static void Construct(Promote &obj, const std::initializer_list<const char *> &syms);
  static void MoveConstruct(Promote &obj, Promote &&other) noexcept;
  static Promote &MoveAssign(Promote &obj, Promote &&other) noexcept;
  static std::vector<const char *> Syms(const Promote &obj);
};
}  // namespace ge

#endif  // BASE_TYPE_TYPES_IMPL_H
