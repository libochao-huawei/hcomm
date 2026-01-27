#include "hccl_api_base_test.h"

class HcclCommInitAllTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        devs = 8;
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
protected:
    int devs;
};

TEST_F(HcclCommInitAllTest, Ut_HcclCommInitAll_When_NDevIsZero_Expect_ReturnIsHCCL_E_PARA)
{
    const uint32_t ndev = 0;
    int32_t devices[devs] = {0, 1, 2, 3, 4, 5, 6, 7};
    HcclComm comms[devs] = {};

    for(int i = 0;i < devs;i ++) {
        HcclResult ret = hrtSetDevice(devices[i]);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    HcclResult ret = HcclCommInitAll(ndev, devices, comms);
    EXPECT_EQ(ret, HCCL_E_PARA);

    for (int i = 0; i < devs; i++) {
        HcclResult ret = hrtResetDevice(devices[i]);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        Ut_Comm_Destroy(comms[i]);
    }
}

TEST_F(HcclCommInitAllTest, Ut_HcclCommInitAll_When_DevicesIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    const uint32_t ndev = 8;
    int32_t* pDevices = nullptr;
    HcclComm comms[devs] = {};

    HcclResult ret = HcclCommInitAll(ndev, pDevices, comms);
    EXPECT_EQ(ret, HCCL_E_PTR);

    for (int i = 0; i < devs; i++) {
        Ut_Comm_Destroy(comms[i]);
    }
}

TEST_F(HcclCommInitAllTest, Ut_HcclCommInitAll_When_CommsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    const uint32_t ndev = 8;
    int32_t devices[devs] = {0, 1, 2, 3, 4, 5, 6, 7};
    HcclComm *pComms = nullptr;

    for(int i = 0;i < devs;i ++) {
        HcclResult ret = hrtSetDevice(devices[i]);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    HcclResult ret = HcclCommInitAll(ndev, devices, pComms);
    EXPECT_EQ(ret, HCCL_E_PTR);

    for (int i = 0; i < devs; i++) {
        HcclResult ret = hrtResetDevice(devices[i]);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}

TEST_F(HcclCommInitAllTest, Ut_HcclCommInitAll_When_2Server4Rank_Expect_ReturnHCCL_SUCCESS)
{
    const uint32_t ndev = 8;
    int32_t devices[devs] = {0, 1, 2, 3, 4, 5, 6, 7};
    HcclComm comms[devs] = {};

    for(int i = 0;i < devs;i ++) {
        HcclResult ret = hrtSetDevice(devices[i]);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    HcclResult ret = HcclCommInitAll(ndev, devices, comms);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (int i = 0; i < devs; i++) {
        HcclResult ret = hrtResetDevice(devices[i]);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        Ut_Comm_Destroy(comms[i]);
    }
}