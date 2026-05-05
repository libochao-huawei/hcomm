/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "hccl_comm_pub.h"
#define private public
#include "ccuTaskException.h"
#undef private

using namespace hccl;
using namespace hcomm;

class CcuTaskExceptionTest : public testing::Test
{
protected:
    virtual void SetUp() override {}

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(CcuTaskExceptionTest, Ut_GetChannleIdByCcuErrorInfo)
{
    uint16_t remPostSemChannleId = 1;
    CcuErrorInfo remPostSem;
    remPostSem.repType = CcuRep::CcuRepType::REM_POST_SEM;
    remPostSem.msg.waitSignal.channelId[0] = remPostSemChannleId;
    EXPECT_EQ(CcuTaskException::GetChannleIdByCcuErrorInfo(remPostSem), remPostSemChannleId);
    
    uint16_t readChannleId = 2;
    CcuErrorInfo read;
    read.repType = CcuRep::CcuRepType::READ;
    read.msg.transMem.channelId = readChannleId;
    EXPECT_EQ(CcuTaskException::GetChannleIdByCcuErrorInfo(read), readChannleId);

    uint16_t bufReadChannleId = 3;
    CcuErrorInfo bufRead;
    bufRead.repType = CcuRep::CcuRepType::BUF_READ;
    bufRead.msg.bufTransMem.channelId = bufReadChannleId;
    EXPECT_EQ(CcuTaskException::GetChannleIdByCcuErrorInfo(bufRead), bufReadChannleId);

    CcuErrorInfo loop;
    bufRead.repType = CcuRep::CcuRepType::LOOP;
    EXPECT_EQ(CcuTaskException::GetChannleIdByCcuErrorInfo(loop), 65535);
}