/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>

#include "ccu/ccu_res.h"
#include "hccl/hccl_ccu_res.h"

namespace {
constexpr uint32_t DIE_ID = 0;
constexpr uint32_t DESC_NUM = 1;
constexpr uint32_t DIE_NUM = 1;
constexpr uint32_t KERNEL_ARG_NUM = 1;
constexpr uint32_t RES_NUM = 1;
constexpr HcommCcuResDescHandle RES_DESC = 1;
constexpr CcuInsHandle CCU_INS_HANDLE = 1;
} // namespace

TEST(HcommCcuResStubApiTest, Ut_HcommCcuInsResDescCreate_WhenCalled_ExpectNotSupport)
{
    HcommCcuResDescHandle resDesc = RES_DESC;

    EXPECT_EQ(HcommCcuInsResDescCreate(DIE_ID, &resDesc), CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuInsResDescDestroy_WhenCalled_ExpectNotSupport)
{
    EXPECT_EQ(HcommCcuInsResDescDestroy(RES_DESC), CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuInsResDescQueryDieId_WhenCalled_ExpectNotSupport)
{
    uint32_t dieId = DIE_ID;

    EXPECT_EQ(HcommCcuInsResDescQueryDieId(RES_DESC, &dieId), CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuInsResDescSetNum_WhenCalled_ExpectNotSupport)
{
    EXPECT_EQ(HcommCcuInsResDescSetNum(RES_DESC, HCOMM_CCU_RES_TYPE_LOOP, RES_NUM),
        CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuInsResDescQueryNum_WhenCalled_ExpectNotSupport)
{
    uint32_t resNum = RES_NUM;

    EXPECT_EQ(HcommCcuInsResDescQueryNum(RES_DESC, HCOMM_CCU_RES_TYPE_LOOP, &resNum),
        CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuInsCreate_WhenCalled_ExpectNotSupport)
{
    HcommCcuResDescHandle resDescs[DESC_NUM] = {RES_DESC};
    CcuInsHandle insHandle = CCU_INS_HANDLE;

    EXPECT_EQ(HcommCcuInsCreate(resDescs, DESC_NUM, &insHandle), CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuInsCreateDefault_WhenCalled_ExpectNotSupport)
{
    uint32_t dieIds[DIE_NUM] = {DIE_ID};
    CcuInsHandle insHandle = CCU_INS_HANDLE;

    EXPECT_EQ(HcommCcuInsCreateDefault(dieIds, DIE_NUM, &insHandle), CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuInsDestroy_WhenCalled_ExpectNotSupport)
{
    EXPECT_EQ(HcommCcuInsDestroy(CCU_INS_HANDLE), CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuInsQueryResDesc_WhenCalled_ExpectNotSupport)
{
    EXPECT_EQ(HcommCcuInsQueryResDesc(CCU_INS_HANDLE, RES_DESC), CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuQueryRemainResDesc_WhenCalled_ExpectNotSupport)
{
    EXPECT_EQ(HcommCcuQueryRemainResDesc(RES_DESC), CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcommCcuKernelQueryResReq_WhenCalled_ExpectNotSupport)
{
    uint32_t kernelFunc = 0;
    const void *kernelArgs[KERNEL_ARG_NUM] = {nullptr};

    EXPECT_EQ(HcommCcuKernelQueryResReq(&kernelFunc, kernelArgs, KERNEL_ARG_NUM, RES_DESC),
        CcuResult::CCU_E_NOT_SUPPORT);
}

TEST(HcommCcuResStubApiTest, Ut_HcclCommAssignCcuIns_WhenCalled_ExpectNotSupport)
{
    uint32_t commStorage = 0;
    HcclComm comm = &commStorage;

    EXPECT_EQ(HcclCommAssignCcuIns(comm, CCU_INS_HANDLE), HcclResult::HCCL_E_NOT_SUPPORT);
}
