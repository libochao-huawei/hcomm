#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "coll_comm_mgr.h"
#include "coll_comm.h"

using namespace hccl;

class CollCommMgrTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "CollCommMgrTest set up." << std::endl;
    }
    static void TearDownTestCase() {
        std::cout << "CollCommMgrTest tear down." << std::endl;
    }
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(CollCommMgrTest, Ut_GetInstance_NotNull) {
    auto mgr = CollCommMgr::GetInstance();
    EXPECT_NE(mgr, nullptr);
}

TEST_F(CollCommMgrTest, Ut_RegisteAndUnRegisteCollComm_Works) {
    CollCommMgr *mgr = CollCommMgr::GetInstance();
    ManagerCallbacks callbacks;
    // create two CollComm objects with distinct ids
    CollComm comm1(reinterpret_cast<void*>(0x1), 0, std::string("comm_test_1"), callbacks);
    CollComm comm2(reinterpret_cast<void*>(0x2), 1, std::string("comm_test_2"), callbacks);

    // register both
    mgr->RegisteCollComm(&comm1);
    mgr->RegisteCollComm(&comm2);

    auto all = mgr->GetAllCollComms();
    EXPECT_EQ(all.size(), 2);
    EXPECT_NE(all.find("comm_test_1"), all.end());
    EXPECT_NE(all.find("comm_test_2"), all.end());

    // unregister comm1
    mgr->UnRegisteCollComm(&comm1);
    all = mgr->GetAllCollComms();
    EXPECT_EQ(all.size(), 1);
    EXPECT_EQ(all.find("comm_test_1"), all.end());
    EXPECT_NE(all.find("comm_test_2"), all.end());

    // unregister comm2
    mgr->UnRegisteCollComm(&comm2);
    all = mgr->GetAllCollComms();
    EXPECT_EQ(all.size(), 0);
}

TEST_F(CollCommMgrTest, Ut_RegisteSameComm_Overwrites) {
    CollCommMgr *mgr = CollCommMgr::GetInstance();
    ManagerCallbacks callbacks;
    CollComm commA(reinterpret_cast<void*>(0x3), 2, std::string("comm_dup"), callbacks);
    CollComm commB(reinterpret_cast<void*>(0x4), 3, std::string("comm_dup"), callbacks);

    mgr->RegisteCollComm(&commA);
    auto all = mgr->GetAllCollComms();
    EXPECT_EQ(all.size(), 1);
    EXPECT_NE(all.find("comm_dup"), all.end());
    EXPECT_EQ(all["comm_dup"], &commA);

    // register another CollComm with same id -> map should be updated
    mgr->RegisteCollComm(&commB);
    all = mgr->GetAllCollComms();
    EXPECT_EQ(all.size(), 1);
    EXPECT_EQ(all["comm_dup"], &commB);

    // cleanup
    mgr->UnRegisteCollComm(&commB);
}
