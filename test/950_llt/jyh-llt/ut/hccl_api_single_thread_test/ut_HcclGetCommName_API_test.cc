#include "hccl_api_base_test.h"

class HcclGetCommNameTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclGetCommNameTest, Ut_HcclGetCommName_When_CommNameIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    char *commName = nullptr;
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclComm comm;
    ret = HcclCommInitRootInfo(1, &id, 0, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclGetCommName(comm, commName);
    EXPECT_EQ(ret, HCCL_E_PTR);
    Ut_Comm_Destroy(comm);
}

TEST_F(HcclGetCommNameTest, Ut_HcclGetCommName_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    char *commName = new char[ROOTINFO_INDENTIFIER_MAX_LENGTH];
    HcclComm comm = nullptr;
    HcclResult ret = HcclGetCommName(comm, commName);
    delete[] commName;
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetCommNameTest, HcclGetCommName_When_InputNoInit_Expect_ReturnIsHCCL_E_PTR)
{
    char *commName;
    HcclComm comm;
    HcclResult ret = HcclGetCommName(&comm, commName);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetCommNameTest, HcclGetCommName_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    const uint32_t ndev = 8;
    int32_t devices[ndev] = {0, 1, 2, 3, 4, 5, 6, 7};
    HcclComm comms[ndev];
    HcclResult ret = HcclCommInitAll(ndev, devices, comms);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH];
    ret = HcclGetCommName(comms[0], commName);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (int i = 0; i < ndev; i++) {
        ret = hrtSetDevice(i);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        Ut_Comm_Destroy(comms[i]);
    }
}