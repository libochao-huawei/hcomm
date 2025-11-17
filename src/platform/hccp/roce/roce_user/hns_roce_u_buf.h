/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _HNS_ROCE_U_BUF_H_
#define _HNS_ROCE_U_BUF_H_

// BITMAP_WORD_SIZE defined maximum supported size is 10K
#define BITMAP_WORD_LEN  32
#define BITMAP_WORD_NUM  320
#define BITMAP_WORD_SIZE (BITMAP_WORD_LEN * BITMAP_WORD_NUM)
#define PAGE_ALIGN_4KB   (4 * 1024)
#define PAGE_ALIGN_2MB   (2 * 1024 * 1024)

#endif // _HNS_ROCE_U_BUF_H_
