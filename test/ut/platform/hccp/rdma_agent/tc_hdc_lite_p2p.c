/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ut_dispatch.h"
#include <stdlib.h>
#include <string.h>
#include "ra_hdc_lite.h"
#include "ra_comm.h"
#include "tc_hdc.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef unsigned long long u64;
typedef signed int s32;

static struct rdma_lite_cq g_fakeLiteCq;
static struct rdma_lite_device_cq_attr g_fakeDeviceCqAttr;

/* ===================================================================
 * TcHdcLiteTypicalCqStoreFindRemove - Tests Store/Find/FindAttr/Remove
 * =================================================================== */
void TcHdcLiteTypicalCqStoreFindRemove(void)
{
    /* Clean up any existing entries by calling store+remove */
    /* Test 1: FindTypicalCq when list is empty */
    struct rdma_lite_cq *foundCq = RaHdcLiteFindTypicalCq(0, 100);
    EXPECT_ADDR_EQ(foundCq, NULL);

    /* Test 2: FindTypicalCqAttr when list is empty */
    struct rdma_lite_device_cq_attr *foundAttr = NULL;
    int ret = RaHdcLiteFindTypicalCqAttr(0, 100, &foundAttr);
    EXPECT_INT_EQ(ret, -EINVAL);

    /* Test 3: RemoveTypicalCq when list is empty - should not crash */
    RaHdcLiteRemoveTypicalCq(0, 100);

    /* Test 4: Store then Find */
    g_fakeDeviceCqAttr.cqn = 42;
    g_fakeDeviceCqAttr.depth = 256;
    RaHdcLiteStoreTypicalCq(0, 42, &g_fakeLiteCq, &g_fakeDeviceCqAttr);

    foundCq = RaHdcLiteFindTypicalCq(0, 42);
    EXPECT_ADDR_EQ(foundCq, &g_fakeLiteCq);

    /* Test 5: FindTypicalCq with wrong cqn */
    foundCq = RaHdcLiteFindTypicalCq(0, 999);
    EXPECT_ADDR_EQ(foundCq, NULL);

    /* Test 6: FindTypicalCqAttr success */
    foundAttr = NULL;
    ret = RaHdcLiteFindTypicalCqAttr(0, 42, &foundAttr);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(foundAttr->cqn, 42);
    EXPECT_INT_EQ(foundAttr->depth, 256);

    /* Test 7: FindTypicalCqAttr with wrong cqn */
    foundAttr = NULL;
    ret = RaHdcLiteFindTypicalCqAttr(0, 999, &foundAttr);
    EXPECT_INT_EQ(ret, -EINVAL);

    /* Test 8: Store with same phyId/cqn - should update */
    g_fakeDeviceCqAttr.cqn = 42;
    g_fakeDeviceCqAttr.depth = 1024;
    static struct rdma_lite_cq anotherCq;
    RaHdcLiteStoreTypicalCq(0, 42, &anotherCq, &g_fakeDeviceCqAttr);
    foundCq = RaHdcLiteFindTypicalCq(0, 42);
    EXPECT_ADDR_EQ(foundCq, &anotherCq);

    /* Test 9: Store another phyId */
    g_fakeDeviceCqAttr.cqn = 77;
    g_fakeDeviceCqAttr.depth = 512;
    RaHdcLiteStoreTypicalCq(1, 77, &g_fakeLiteCq, &g_fakeDeviceCqAttr);
    foundCq = RaHdcLiteFindTypicalCq(1, 77);
    EXPECT_ADDR_EQ(foundCq, &g_fakeLiteCq);

    /* Test 10: Verify original entry still exists */
    foundCq = RaHdcLiteFindTypicalCq(0, 42);
    EXPECT_ADDR_EQ(foundCq, &anotherCq);

    /* Test 11: Remove existing entry */
    RaHdcLiteRemoveTypicalCq(0, 42);
    foundCq = RaHdcLiteFindTypicalCq(0, 42);
    EXPECT_ADDR_EQ(foundCq, NULL);

    /* Test 12: Remove non-existing entry - should not crash */
    RaHdcLiteRemoveTypicalCq(0, 999);

    /* Test 13: Remove remaining entry */
    RaHdcLiteRemoveTypicalCq(1, 77);
    foundCq = RaHdcLiteFindTypicalCq(1, 77);
    EXPECT_ADDR_EQ(foundCq, NULL);

    /* Test 14: Verify list is effectively empty */
    foundCq = RaHdcLiteFindTypicalCq(0, 42);
    EXPECT_ADDR_EQ(foundCq, NULL);
    foundCq = RaHdcLiteFindTypicalCq(1, 77);
    EXPECT_ADDR_EQ(foundCq, NULL);
}
