/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include <stdlib.h>
#include "ut_dispatch.h"
#include "securec.h"
#include "hccp_nda.h"
#include "ra_rs_err.h"
#include "ra.h"
#include "ra_peer_nda.h"
#include "rs_nda.h"
#include "tc_ra_nda.h"

void TcRaNdaGetDirectFlag()
{
    struct RaRdmaHandle rdmaHandle = {0};
    int directFlag = 0;
    int ret = 0;

    ret = RaNdaGetDirectFlag(NULL, &directFlag);
    EXPECT_INT_EQ(128103, ret);

    ret = RaNdaGetDirectFlag(&rdmaHandle, &directFlag);
    EXPECT_INT_EQ(0, ret);
}

void TcRaPeerNdaGetDirectFlag()
{
    struct RaRdmaHandle rdmaHandle = {0};
    int directFlag = 0;
    int ret = 0;

    mocker_clean();
    mocker(RsNdaGetDirectFlag, 1, -1);
    ret = RaPeerNdaGetDirectFlag(&rdmaHandle, &directFlag);
    EXPECT_INT_EQ(-1, ret);
}

