#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#include "ns_recovery_lite.h"

using namespace hccl;

TEST(NsRecoveryLiteTest, DefaultAndNeedCleanFlag)

{
    NsRecoveryLite lite;
    // 默认不需要清理
    EXPECT_FALSE(lite.IsNeedClean());

    // 设置需要清理
    lite.SetNeedClean(true);
    EXPECT_TRUE(lite.IsNeedClean());

    // 取消需要清理
    lite.SetNeedClean(false);
    EXPECT_FALSE(lite.IsNeedClean());
}

TEST(NsRecoveryLiteTest, ResetErrorReportedDoesNotCrash)

{
    NsRecoveryLite lite;
    // ResetErrorReported 只是将内部标识复位，不应引发异常或崩溃
    lite.ResetErrorReported();
    SUCCEED();
}

// 注意：BackGroundGetCmd/BackGroundSetStatus 依赖于内部的 hdcHandler_
// 必须先通过 Init 提供有效的 HDCommunicate 对象后才能调用。当前测试避免对未初始化的 hdcHandler_ 调用以防崩溃。
