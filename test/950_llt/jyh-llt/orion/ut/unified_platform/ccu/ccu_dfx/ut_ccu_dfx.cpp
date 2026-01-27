#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mockcpp/mokc.h>

#include "ccu_dfx.h"
#include "ccu_context_mgr_imp.h"
#include "ccu_ctx.h"
#include "task_param.h"
#include "exception_util.h"
#include "ccu_api_exception.h"

#define private public
#define protected public
#include "ccu_error_handler.h"
#undef private
#undef protected


using namespace std;
using namespace Hccl;

class CcuDfxTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CcuDfxTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "CcuDfxTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in CcuDfxTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in CcuDfxTest TearDown" << std::endl;
    }
};

TEST_F(CcuDfxTest, get_ccu_error_msg_should_fail_when_param_error)
{
    ParaCcu ccuTaskParam;
    ccuTaskParam.dieId = 0;
    ccuTaskParam.missionId = 0;
    ccuTaskParam.executeId = 0;
    vector<CcuErrorInfo> errorInfo{};
    EXPECT_EQ(GetCcuErrorMsg(100, ccuTaskParam, errorInfo), HcclResult::HCCL_E_PARA);
}

void GetCcuErrorMsgExcptionStub(int32_t deviceId, const ParaCcu &ccuTaskParam, vector<CcuErrorInfo> &errorInfo)
{
    THROW<CcuApiException>("API failed.");
}

TEST_F(CcuDfxTest, get_ccu_error_msg_should_fail_when_throw_exception)
{
    ParaCcu ccuTaskParam;
    ccuTaskParam.dieId = 0;
    ccuTaskParam.missionId = 0;
    ccuTaskParam.executeId = 0;
    vector<CcuErrorInfo> errorInfo{};

    MOCKER_CPP(&CtxMgrImp::Init).stubs();
    CcuContext* ctx{nullptr};
    MOCKER_CPP(&CtxMgrImp::GetCtx).stubs().with(any(), any(), any()).will(returnValue(ctx));
    CcuMissionContext missionCtx{};
    missionCtx.part2.value = 0xffff;
    missionCtx.part3.value = 0xffff;
    MOCKER(CcuErrorHandler::GetCcuMissionContext).stubs().with(any(), any(), any()).will(returnValue(missionCtx));

    EXPECT_EQ(GetCcuErrorMsg(1, ccuTaskParam, errorInfo), HcclResult::HCCL_E_INTERNAL);
}