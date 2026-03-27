#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#include "ns_recovery_func_lite.h"

using namespace hccl;

TEST(NsRecoveryFuncLiteTest, DeviceQuery_DrvError)
{
    // 模拟 halTsdrvCtl 返回驱动不支持
    MOCKER(halTsdrvCtl).stubs().with(any()).will(returnValue(DRV_ERROR_NOT_SUPPORT));
    auto ret = NsRecoveryFuncLite::GetInstance().DeviceQuery(0, 0, 0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_DRV);
    GlobalMockObject::verify();
    GlobalMockObject::reset();
}

TEST(NsRecoveryFuncLiteTest, DeviceQuery_Success)
{
    // 模拟 halTsdrvCtl 返回成功
    MOCKER(halTsdrvCtl).stubs().with(any()).will(returnValue(DRV_ERROR_NONE));
    auto ret = NsRecoveryFuncLite::GetInstance().DeviceQuery(0, 0, 0);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
    GlobalMockObject::verify();
    GlobalMockObject::reset();
}
