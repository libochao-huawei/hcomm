#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "ccu_dev_mgr_pub.h"
#include "ccu_comp.h"
#include "hccl_common.h"
// #include "ccu_res_batch_allocator.h"

// [新增] 添加 CcuPfeCfgMgr 的头文件
#include "ccu_pfe_cfg_mgr.h" 
#include "ccu_res_batch_allocator.h"
#include "ccu_res_specs.h"
#include <cstdint>
#include "unified_platform/ccu/ccu_device/ccu_component/ccu_component.h"

#define private public

using namespace hcomm;

class CcuCompPubTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }
    static void TearDownTestCase() {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }
    void SetUp() override {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }
    void TearDown() override {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }

    // Helpers to stub CcuComponent member functions
    void StubSetTaskKill(const HcclResult ret) {
        MOCKER_CPP(&CcuComponent::SetTaskKill).stubs().will(returnValue(ret));
        MOCKER_CPP(&Hccl::CcuComponent::SetTaskKill).stubs().will(returnValue(ret));
    }
    void StubSetTaskKillDone(const HcclResult ret) {
        MOCKER_CPP(&CcuComponent::SetTaskKillDone).stubs().will(returnValue(ret));
        MOCKER_CPP(&Hccl::CcuComponent::SetTaskKillDone).stubs().will(returnValue(ret));
    }
    void StubCleanTaskKillState(const HcclResult ret) {
        MOCKER_CPP(&CcuComponent::CleanTaskKillState).stubs().will(returnValue(ret));
        MOCKER_CPP(&Hccl::CcuComponent::CleanTaskKillState).stubs().will(returnValue(ret));
    }
    void StubCleanDieCkes(const HcclResult ret) {
        MOCKER_CPP(&CcuComponent::CleanDieCkes).stubs().with(any()).will(returnValue(ret));
        MOCKER_CPP(&Hccl::CcuComponent::CleanDieCkes).stubs().with(any()).will(returnValue(ret));
    }
};

// CcuSetTaskKill: invalid deviceLogicId -> HCCL_E_PARA
TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillWhenDeviceIdInvalidExpectHcclEPara) {
    const int32_t badId = static_cast<int32_t>(MAX_MODULE_DEVICE_NUM); // out of range
    auto ret = CcuSetTaskKill(-1);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
    ret = CcuSetTaskKill(badId);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
}

// CcuSetTaskKill: valid deviceLogicId forwards to CcuComponent::SetTaskKill
TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillWhenUnderlyingSucceedsExpectSuccess) {
    StubSetTaskKill(HcclResult::HCCL_SUCCESS);
    auto ret = CcuSetTaskKill(0);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillWhenUnderlyingFailsExpectFailure) {
    StubSetTaskKill(HcclResult::HCCL_E_INTERNAL);
    auto ret = CcuSetTaskKill(0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_INTERNAL);
}

// CcuSetTaskKillDone: invalid deviceLogicId -> HCCL_E_PARA
TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillDoneWhenDeviceIdInvalidExpectHcclEPara) {
    auto ret = CcuSetTaskKillDone(-2);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
    ret = CcuSetTaskKillDone(static_cast<int32_t>(MAX_MODULE_DEVICE_NUM));
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
}

// CcuSetTaskKillDone: forwards to CcuComponent::SetTaskKillDone
TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillDoneWhenUnderlyingSucceedsExpectSuccess) {
    StubSetTaskKillDone(HcclResult::HCCL_SUCCESS);
    auto ret = CcuSetTaskKillDone(0);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillDoneWhenUnderlyingFailsExpectFailure) {
    StubSetTaskKillDone(HcclResult::HCCL_E_INTERNAL);
    auto ret = CcuSetTaskKillDone(0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_INTERNAL);
}

// CcuCleanTaskKillState: invalid deviceLogicId -> HCCL_E_PARA
TEST_F(CcuCompPubTest, Ut_CcuCleanTaskKillStateWhenDeviceIdInvalidExpectHcclEPara) {
    auto ret = CcuCleanTaskKillState(-5);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
    ret = CcuCleanTaskKillState(static_cast<int32_t>(MAX_MODULE_DEVICE_NUM));
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
}

// CcuCleanTaskKillState: forwards to CcuComponent::CleanTaskKillState
TEST_F(CcuCompPubTest, Ut_CcuCleanTaskKillStateWhenUnderlyingSucceedsExpectSuccess) {
    StubCleanTaskKillState(HcclResult::HCCL_SUCCESS);
    auto ret = CcuCleanTaskKillState(0);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuCompPubTest, Ut_CcuCleanTaskKillStateWhenUnderlyingFailsExpectFailure) {
    StubCleanTaskKillState(HcclResult::HCCL_E_INTERNAL);
    auto ret = CcuCleanTaskKillState(0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_INTERNAL);
}

// CcuCleanDieCkes: invalid deviceLogicId -> HCCL_E_PARA
TEST_F(CcuCompPubTest, Ut_CcuCleanDieCkesWhenDeviceIdInvalidExpectHcclEPara) {
    auto ret = CcuCleanDieCkes(-1, 0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
    ret = CcuCleanDieCkes(static_cast<int32_t>(MAX_MODULE_DEVICE_NUM), 1);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
}

// CcuCleanDieCkes: forwards to CcuComponent::CleanDieCkes
TEST_F(CcuCompPubTest, Ut_CcuCleanDieCkesWhenUnderlyingSucceedsExpectSuccess) {
    StubCleanDieCkes(HcclResult::HCCL_SUCCESS);
    auto ret = CcuCleanDieCkes(0, 1);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuCompPubTest, Ut_CcuCleanDieCkesWhenUnderlyingFailsExpectFailure) {
    StubCleanDieCkes(HcclResult::HCCL_E_INTERNAL);
    auto ret = CcuCleanDieCkes(0, 1);
    EXPECT_EQ(ret, HcclResult::HCCL_E_INTERNAL);
}


TEST(CcuPfeCfgMgrSimpleTest, Ut_InvalidDieId) {
    auto& mgr = CcuPfeCfgMgr::GetInstance(0);
    // 传入无效 dieId，验证返回空向量以覆盖 warning 分支
    EXPECT_TRUE(mgr.GetPfeJettyCtxCfg(CCU_MAX_IODIE_NUM).empty());
}

TEST(CcuResBatchAllocatorTest, Ut_MissionMgrAllocWithUnsupportedReqTypeExpectWarning) {
    hcomm CcuResBatchAllocator allocat{}; 
    uintptr_t handleKey = 0;
    MissionReq missionReq; 
    missionReq.reqType = MissionReqType::COMM_ENGINE_RESERVED; 
    misssionReq.req[0] = 0;
    misssionReq.req[0] = 1;
    MissionResInfo missionInfos{};
    allocat.missionMgr_.Alloc(handleKey, missionReq, missionInfos);
}


/**
 * 测试用例 2: 覆盖第 786 行 HCCL_WARNING 分支
 * 场景: Mission 资源不足，HandleBlockRes 返回 HCCL_E_UNAVAIL
 * 
 HcclResult CcuResBatchAllocator::CcuMissionMgr::Alloc(const uintptr_t handleKey,
    const MissionReq &missionReq, MissionResInfo &missionInfos)
{
    MissionReqType reqType = missionReq.reqType;
    constexpr MissionReqType defaultReqType = MissionReqType::FUSION_MULTIPLE_DIE;
    if (missionReq.reqType != MissionReqType::FUSION_MULTIPLE_DIE) {
        HCCL_WARNING("[CcuMissionMgr][%s] mission reqType[%d], mission resources "
            "now only support %d.", __func__, reqType,
            defaultReqType);
        reqType = MissionReqType::FUSION_MULTIPLE_DIE;
    }

 */
// TEST(CcuResBatchAllocatorTest, Ut_MissionMgrAllocWhenResourceUnavailableExpectHcclEUnavail) {
   
    
//     int32_t devLogicId = 0;
//     auto& allocator = CcuResBatchAllocator::GetInstance(devLogicId);
    
//     // 构造一个请求大量 Mission 资源的 Req，超过预分配的大小
//     CcuResReq req{};
//     req.missionReq.reqType = MissionReqType::FUSION_MULTIPLE_DIE;
//     // 假设预分配的 block 很少，请求一个巨大的数量以触发 E_UNAVAIL
//     req.missionReq.req[0] = 10000; 
//     req.missionReq.req[1] = 10000;

//     CcuResHandle handle = nullptr;
    
//     // 执行分配，预期返回 HCCL_E_UNAVAIL
//     auto ret = allocator.AllocResHandle(req, handle);
    
//     EXPECT_EQ(ret, HcclResult::HCCL_E_UNAVAIL);
//     EXPECT_EQ(handle, nullptr);
// }