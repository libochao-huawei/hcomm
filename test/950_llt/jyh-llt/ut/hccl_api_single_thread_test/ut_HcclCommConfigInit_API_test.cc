#include "hccl_api_base_test.h"

class HcclCommConfigInitTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommConfigInitTest,Ut_HcclCommConfigInit_When_Normal_Expect_Normal)
{
    Ut_Device_Set(0);
    const char* rankTableFile = rankTableFileName;
    u32 rankId = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize=400;
    HcclComm comm = nullptr;
    HcclResult ret = HcclCommInitClusterInfoConfig(rankTableFile, rankId, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Ut_Comm_CheckRet_Then_Destroy(ret,comm);
}


