#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>


#include "../../../../../src/framework/next/comms/ccu/ccu_device/ccu_res_batch_allocator.h"
#include "ccu_dev_mgr_pub.h"
#include "ccu_comp.h"
#include "hccl_common.h"

#define private public
#define protected public

using namespace hcomm;

class CcuResBatchAllocatorTest : public testing::Test {
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

};

TEST_F(CcuResBatchAllocatorTest, Alloc_WhenReqTypeNotDefault_ShouldForceSetToDefault)
{   
    hcomm::CcuResBatchAllocator allocat{};
    uintptr_t handleKey;
    MissionReq missionReq;
    missionReq.reqType = MissionReqType::COMM_ENGINE_RESERVED; 
    missionReq.req[0] = 0;
    missionReq.req[1] = 0;
    MissionResInfo missionInfos;
    
    auto ret = allocat.missionMgr_.Alloc(handleKey, missionReq, missionInfos);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}


TEST_F(CcuResBatchAllocatorTest, Alloc_WhenReqNumIs0_ShouldReturnSuccessDirectly)
{
    hcomm::CcuResBatchAllocator allocat{};
    uintptr_t handleKey;
    MissionReq missionReq;
    missionReq.reqType = MissionReqType::FUSION_MULTIPLE_DIE; 
    missionReq.req[0] = 0;
    missionReq.req[1] = 0;
    MissionResInfo missionInfos;
    
    auto ret = allocat.missionMgr_.Alloc(handleKey, missionReq, missionInfos);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}