#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "null_ptr_exception.h"
#define private public
#include "cnt_nto1_notify_lite_mgr.h"
#include "queue_wait_group_cnt_notify_manager.h"
#undef private

using namespace Hccl;

class CntNto1NotifyLiteMgrTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "CntNto1NotifyLiteMgrTest SetUP" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "CntNto1NotifyLiteMgrTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        std::cout << "A Test case in CntNto1NotifyLiteMgrTest SetUP" << std::endl;
    }

    virtual void TearDown () {
        GlobalMockObject::verify();
        std::cout << "A Test case in CntNto1NotifyLiteMgrTest TearDown" << std::endl;
    }
};

TEST_F(CntNto1NotifyLiteMgrTest, test_parse_packed_data)
{
    QueueWaitGroupCntNotifyManager cntNto1NotifyMgr;
    CntNto1NotifyLiteMgr           mgr;

    MOCKER(HrtGetDevice)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtNotifyCreate)
            .stubs()
            .will(returnValue((void*)(0)));
    MOCKER(HrtGetDevicePhyIdByIndex)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtGetNotifyID)
            .stubs()
            .will(returnValue(1))
            .then(returnValue(2))
            .then(returnValue(3));

    QId fakePostQid = 1;
    u32 fakeTopicId = 2;
    cntNto1NotifyMgr.ApplyFor(fakePostQid, fakeTopicId);
    fakePostQid = 2;
    fakeTopicId = 3;
    cntNto1NotifyMgr.ApplyFor(fakePostQid, fakeTopicId);
    fakePostQid = 3;
    fakeTopicId = 4;
    cntNto1NotifyMgr.ApplyFor(fakePostQid, fakeTopicId);

    auto data = cntNto1NotifyMgr.GetPackedData();
    mgr.ParsePackedData(data);

    EXPECT_EQ(cntNto1NotifyMgr.notifyPool.size(), mgr.notifys.size());
    for (auto &it : cntNto1NotifyMgr.notifyPool) {
        auto &rtsCntNotify = *(it.second);
        auto &cntNto1NotifyLite = mgr.notifys[it.first];
        EXPECT_EQ(rtsCntNotify.GetId(), cntNto1NotifyLite->GetId());
    }
}