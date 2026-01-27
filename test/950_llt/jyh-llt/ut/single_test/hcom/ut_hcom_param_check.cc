#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <string>


#include "param_check_pub.h"
#include "externalinput_pub.h"
#include "sal.h"
#include <string>

using namespace std;
//using namespace hccl;


class HcomParamCheck : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--HcomParamCheck SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--HcomParamCheck TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};
TEST_F(HcomParamCheck, hcom_ranktable_check1)
{
    const char *ranktable1 = "fsdgfsdagsdagdsafweqqq";
    std::string realPath;
    HcclResult ret = HcomGetRanktableRealPath(ranktable1, realPath);;
    EXPECT_EQ(ret, HCCL_E_PARA);
    const char *ranktable2 = "/usr/X11R6/lib/modules/../../include/../X11R6/lib/modules/../../include/../X11R6/lib/modules/../../include/../X11R6/lib/modules/../../include/../X11R6/lib/modules/../../include/../X11R6/lib/modules/../../include/../";
    ret = HcomGetRanktableRealPath(ranktable2, realPath);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcomParamCheck, hcom_identify_check1)
{
    const char *identify1 = "fsdgfsdagsdagdsafweqqqrfdsgfdxcvxcbvfdghfsadsadsafsdgdsgasfgsdgfcvbvcbdsfgfdgds1111#$%^&&&$fsdgfsdagsdagdsafweqqqrfdsgfdxcvxcbvfdghfsadsadsafsdgdsgasfgsdgfcvbvcbdsfgfdgds1111#$%^&&&$";
    HcclResult ret = HcomCheckIdentify(identify1);
    EXPECT_EQ(ret, HCCL_E_PARA);
}
TEST_F(HcomParamCheck, hcom_device_id_check1)
{
    const u32 device_id1 = 9;
    HcclResult ret = HcomCheckDeviceId(device_id1);
    EXPECT_EQ(ret, HCCL_E_PARA);
}
TEST_F(HcomParamCheck, hcom_tag_check1)
{
    const char *tag1 = "fsdgfsdagsdagdsafweqqqrfdsgfdxcvxcbvfdghfsadsadsafsdgdsgasfgsdgfcvbvcbdsfgfdgdfsdgfsdagsdagdsafweqqqrfdsgfdxcvxcbvfdghfsadsadsafsdgdsgasfgsdgfcvbvcbdsfgfdgds1111#$%^&&&$s1111#$%^&&&$12312345322345";
    HcclResult ret = HcomCheckTag(tag1);
    EXPECT_EQ(ret, HCCL_E_PARA);
}
TEST_F(HcomParamCheck, hcom_count_check1)
{
    const u64 count1 = 0xFFFFFFFFFFFFFFFF;
    HcclResult ret = HcomCheckCount(count1);
    EXPECT_EQ(ret, HCCL_E_PARA);
}
TEST_F(HcomParamCheck, hcom_data_type_check1)
{
    const HcclDataType dataType = HCCL_DATA_TYPE_RESERVED;
    HcclResult ret = HcomCheckDataType(dataType);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}
TEST_F(HcomParamCheck, hcom_group_name_check1)
{
    const char *group1 = "fsdgfsdagsdagdsafweqqqrfdsfsdgfsdagsdagdsafweqqqrfdsgfdxcvxcbvfdghfsadsadsafsdgdsgasfgsdgfcvbvcbdsfgfdgds1111#$%^&&&$gfdxcvxcbvfdghfsadsadsafsdgdsgasfgsdgfcvbvcbdsfgfdgds1111#$%^&&&$";
    HcclResult ret = HcomCheckGroupName(group1);
    EXPECT_EQ(ret, HCCL_E_PARA);
}
TEST_F(HcomParamCheck, hcom_reduction_op_check1)
{
    const HcclReduceOp op = HCCL_REDUCE_RESERVED;
    const HcclDataType dataType = HCCL_DATA_TYPE_INT32;
    HcclResult ret = HcomCheckReductionOp(op);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(HcomParamCheck, hcom_user_rank_check1)
{
    const u32 totalRanks = 128;
    const u32 userRank = 129;
    HcclResult ret = HcomCheckUserRank(totalRanks, userRank);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcomParamCheck, hcom_HcomCheckReduceDataType)
{
    const HcclReduceOp op = HCCL_REDUCE_PROD;
    HcclDataType dataType = HCCL_DATA_TYPE_INT16;
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HcomCheckReduceDataType(dataType, op, deviceType);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    
    deviceType = DevType::DEV_TYPE_910;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    ret = HcomCheckReduceDataType(dataType, op, deviceType);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    deviceType = DevType::DEV_TYPE_310P3;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    ret = HcomCheckReduceDataType(dataType, op, deviceType);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}