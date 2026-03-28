/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstring>
#include "gtest/gtest.h"
#include "hcomm_c_adpt.h"
#include "hcomm_res_defs.h"

class HcommCAdptTest : public testing::Test {};

TEST_F(HcommCAdptTest, ut_HcommChannelGet_When_ChannelPtrNull_Expect_E_PTR)
{
    ChannelHandle channelHandle = 0x12345;
    HcommResult ret = HcommChannelGet(channelHandle, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGet_When_ChannelNotFound_Expect_E_NOT_FOUND)
{
    ChannelHandle channelHandle = 0x12345;
    void *channel = nullptr;
    HcommResult ret = HcommChannelGet(channelHandle, &channel);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetStatus_When_ChannelListNull_Expect_E_PTR)
{
    int32_t statusList[2] = {0, 0};
    HcommResult ret = HcommChannelGetStatus(nullptr, 2, statusList);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetStatus_When_StatusListNull_Expect_E_PTR)
{
    ChannelHandle channelList[2] = {0x12345, 0x12346};
    HcommResult ret = HcommChannelGetStatus(channelList, 2, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetStatus_When_ListNumZero_Expect_Success)
{
    ChannelHandle channelList[1] = {0x12345};
    int32_t statusList[1] = {0};
    HcommResult ret = HcommChannelGetStatus(channelList, 0, statusList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetNotifyNum_When_ChannelNotFound_Expect_E_NOT_FOUND)
{
    ChannelHandle channelHandle = 0x12345;
    uint32_t notifyNum = 0;
    HcommResult ret = HcommChannelGetNotifyNum(channelHandle, &notifyNum);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(HcommCAdptTest, ut_HcommChannelDestroy_When_ChannelsNull_Expect_E_PTR)
{
    HcommResult ret = HcommChannelDestroy(nullptr, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelDestroy_When_ChannelNotFound_Expect_E_NOT_FOUND)
{
    ChannelHandle channels[1] = {0x12345};
    HcommResult ret = HcommChannelDestroy(channels, 1);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetRemoteMem_When_ChannelNotFound_Expect_E_NOT_FOUND)
{
    ChannelHandle channelHandle = 0x12345;
    CommMem *remoteMem = nullptr;
    uint32_t memNum = 0;
    char *memTagsStorage[1] = {nullptr};
    char **memTags = memTagsStorage;
    HcommResult ret = HcommChannelGetRemoteMem(channelHandle, &remoteMem, &memNum, memTags);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetUserRemoteMem_When_ChannelNotFound_Expect_E_NOT_FOUND)
{
    ChannelHandle channelHandle = 0x12345;
    CommMem *remoteMem = nullptr;
    char **memTag = nullptr;
    uint32_t memNum = 0;
    HcommResult ret = HcommChannelGetUserRemoteMem(channelHandle, &remoteMem, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_When_ChannelDescsNull_Expect_E_PTR)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    ChannelHandle channels[1] = {0};
    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_AICPU_TS, nullptr, 1, channels);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_When_ChannelHandleListNull_Expect_E_PTR)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_AICPU_TS, &channelDesc, 1, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_When_ChannelNumZero_Expect_Success)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};
    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_AICPU_TS, &channelDesc, 0, channels);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommEndpointGet_When_NotFound_Expect_E_PARA)
{
    EndpointHandle handle = reinterpret_cast<EndpointHandle>(0x12345678);
    void *endpoint = nullptr;
    HcommResult ret = HcommEndpointGet(handle, &endpoint);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcommCAdptTest, ut_HcommEndpointGet_When_EndpointPtrNull_AndNotFound_Expect_E_PARA)
{
    EndpointHandle handle = reinterpret_cast<EndpointHandle>(0x12345678);
    HcommResult ret = HcommEndpointGet(handle, nullptr);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcommCAdptTest, ut_HcommEngineCtxCopy_When_CPU_Expect_Success)
{
    char dstCtx[32] = {};
    char srcCtx[32] = {};
    for (size_t i = 0; i < sizeof(srcCtx); ++i) {
        srcCtx[i] = static_cast<char>(i);
    }

    HcommResult ret = HcommEngineCtxCopy(COMM_ENGINE_CPU, dstCtx, srcCtx, sizeof(srcCtx));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(memcmp(dstCtx, srcCtx, sizeof(srcCtx)), 0);
}

TEST_F(HcommCAdptTest, ut_HcommEngineCtxCopy_When_CPU_TS_Expect_Success)
{
    char dstCtx[16] = {};
    char srcCtx[16] = {1, 2, 3};

    HcommResult ret = HcommEngineCtxCopy(COMM_ENGINE_CPU_TS, dstCtx, srcCtx, sizeof(srcCtx));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(memcmp(dstCtx, srcCtx, sizeof(srcCtx)), 0);
}

TEST_F(HcommCAdptTest, ut_HcommEngineCtxCopy_When_CCU_Expect_Success)
{
    char dstCtx[8] = {};
    char srcCtx[8] = {7, 6, 5, 4, 3, 2, 1, 0};

    HcommResult ret = HcommEngineCtxCopy(COMM_ENGINE_CCU, dstCtx, srcCtx, sizeof(srcCtx));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(memcmp(dstCtx, srcCtx, sizeof(srcCtx)), 0);
}

TEST_F(HcommCAdptTest, ut_HcommEngineCtxCopy_When_EngineInvalid_Expect_E_PARA)
{
    char dstCtx[8] = {};
    char srcCtx[8] = {};

    HcommResult ret = HcommEngineCtxCopy(COMM_ENGINE_RESERVED, dstCtx, srcCtx, sizeof(srcCtx));
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcommCAdptTest, ut_HcommChannelDescInit_When_Normal_Expect_Success)
{
    HcommChannelDesc channelDesc{};
    HcommResult ret = HcommChannelDescInit(&channelDesc, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channelDesc.header.magicWord, HCOMM_CHANNEL_MAGIC_WORD);
}

TEST_F(HcommCAdptTest, ut_HcommChannelDescInit_When_ChannelDescNull_Expect_E_PTR)
{
    HcommResult ret = HcommChannelDescInit(nullptr, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
}
