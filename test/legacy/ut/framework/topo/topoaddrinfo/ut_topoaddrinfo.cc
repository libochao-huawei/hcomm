#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <cstdlib>
#include <string>
#include <stdint.h>
#include <ctype.h>
#include "securec.h"
#include "topo_addr_info.h"
#include "hal.h"

/**
 * @brief 将32字符十六进制字符串转为16字节二进制数组
 * @param hex_str  输入：32位十六进制字符串（必须以'\0'结尾）
 * @param bin_out  输出：16字节uint8_t数组
 * @return 成功0，失败-1（非法字符）
 */
int hex32_to_bin16(const char *hex_str, uint8_t *bin_out) {
    for (int i = 0; i < 16; i++) {
        // 取两个十六进制字符
        char c1 = hex_str[2*i];
        char c2 = hex_str[2*i + 1];

        // 转数字（0-15）
        int h1 = tolower((unsigned char)c1);
        int h2 = tolower((unsigned char)c2);

        h1 = (h1 >= '0' && h1 <= '9') ? h1 - '0' :
             (h1 >= 'a' && h1 <= 'f') ? 10 + h1 - 'a' : -1;

        h2 = (h2 >= '0' && h2 <= '9') ? h2 - '0' :
             (h2 >= 'a' && h2 <= 'f') ? 10 + h2 - 'a' : -1;

        // 非法字符检查
        if (h1 < 0 || h2 < 0) return -1;

        // 组合成1字节：高4位 + 低4位
        bin_out[i] = (h1 << 4) | h2;
    }
    return 0;
}

class TopoAddrInfoTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TopoAddrInfo tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "TopoAddrInfo tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in TopoAddrInfoTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in TopoAddrInfoTest TearDown" << std::endl;
    }
};

TEST_F(TopoAddrInfoTest, Ut_get_mainbaord_id)
{
    unsigned int mainboard_id = 0;
    int ret = hal_get_mainboard_id(0, &mainboard_id);
    // check
    EXPECT_EQ(mainboard_id, 0);
    EXPECT_EQ(ret, -1);
}

TEST_F(TopoAddrInfoTest, Ut_Card_2Px)
{
    unsigned int m = 3;
    MOCKER(hal_get_mainboard_id).stubs().with(any(), outBoundP(&m)).will(returnValue(0));

    unsigned int xx = 0;
    int ret = hal_get_mainboard_id(0, &xx);
    EXPECT_EQ(xx, 3);
    EXPECT_EQ(ret, 0);
}

/**
 * 验证标卡2P场景
 */
TEST_F(TopoAddrInfoTest, Ut_Card_2P)
{
    // mock data
    unsigned int m = 0x6A;
    char drv_path[256] = "/usr/local/Ascend2";
    dcmi_urma_eid_info_t eidList[MAX_EID_NUM];
    hex32_to_bin16("000000000000000000100000dfdf0020", eidList[0].eid.raw);
    hex32_to_bin16("000000000000000000100000dfdf0028", eidList[1].eid.raw);
    hex32_to_bin16("000000000000000000100000dfdf0030", eidList[2].eid.raw);
    hex32_to_bin16("000000000000000000100000dfdf0051", eidList[3].eid.raw);
    size_t eidNum = 4;

    MOCKER(hal_get_mainboard_id).stubs().with(any(), outBoundP(&m)).will(returnValue(0));
    MOCKER(hal_get_driver_install_path).stubs().with(outBoundP(drv_path, strlen(drv_path)), any()).will(returnValue(0));
    MOCKER(hal_get_eid_list_by_phy_id).stubs().with(any(), outBoundP(eidList, eidNum*sizeof(dcmi_urma_eid_info_t)), outBoundP(&eidNum)).will(returnValue(0));

    char* buf = (char*)malloc(4096);
    memset(buf, 0x00, 4096);
    size_t bufSize = 4096;
    int x = TopoAddrInfoGet(0, buf, &bufSize);
    EXPECT_EQ(x, 0);
    printf("[%s]\n", buf);
    EXPECT_TRUE(strstr(buf, "dfdf0051") != 0); // 2P使用portgroup通信，验证portgroup的EID在地址信息中
    free(buf);
}

TEST_F(TopoAddrInfoTest, Ut_Card_2P_GetSize)
{
    // mock data
    unsigned int m = 0x6A;
    MOCKER(hal_get_mainboard_id).stubs().with(any(), outBoundP(&m)).will(returnValue(0));
    size_t bufSize = 0;
    int ret = TopoAddrInfoGetSize(0, &bufSize);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bufSize, 2048);
}

TEST_F(TopoAddrInfoTest, Ut_Card_4P)
{
    // mock data
    unsigned int mainboard_id = 0x6C;
    char drv_path[256] = "/usr/local/Ascend2";
    dcmi_urma_eid_info_t eidList[MAX_EID_NUM];
    memset_s(eidList, MAX_EID_NUM * sizeof(dcmi_urma_eid_info_t), 0x00, MAX_EID_NUM * sizeof(dcmi_urma_eid_info_t));

    hex32_to_bin16("000000000000000000100000dfdf0020", eidList[0].eid.raw);
    hex32_to_bin16("000000000000000000100000dfdf0028", eidList[1].eid.raw);
    hex32_to_bin16("000000000000000000100000dfdf0030", eidList[2].eid.raw);
    hex32_to_bin16("000000000000000000100000dfdf0051", eidList[3].eid.raw);
    size_t eidNum = 4;

    MOCKER(hal_get_mainboard_id).stubs().with(any(), outBoundP(&mainboard_id)).will(returnValue(0));
    MOCKER(hal_get_driver_install_path).stubs().with(outBoundP(drv_path, strlen(drv_path)), any()).will(returnValue(0));
    MOCKER(hal_get_eid_list_by_phy_id).stubs().with(any(), outBoundP(eidList, eidNum*sizeof(dcmi_urma_eid_info_t)), outBoundP(&eidNum)).will(returnValue(0));

    char* buf = (char*)malloc(4096);
    memset(buf, 0x00, 4096);
    size_t bufSize = 4096;
    int ret = TopoAddrInfoGet(0, buf, &bufSize);
    EXPECT_EQ(ret, 0);
    printf("[%s]\n", buf);
    // 4P使用直连口
    EXPECT_TRUE(strstr(buf, "dfdf0051") ==  NULL);
    free(buf);
}

TEST_F(TopoAddrInfoTest, ut_get_pod_rootinfo_size)
{
    unsigned int mainboardId = 0x07;
    const size_t exceptedSize = 2048;
    MOCKER(hal_get_mainboard_id).stubs().with(any(), outBoundP(&mainboardId)).will(returnValue(0));
    size_t size = 0;
    int ret = TopoAddrInfoGetSize(0, &size);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(size, exceptedSize);
}

TEST_F(TopoAddrInfoTest, ut_get_unknow_rootinfo_size)
{
    unsigned int mainboardId = 0x100;
    const size_t exceptedSize = 4096;
    MOCKER(hal_get_mainboard_id).stubs().with(any(), outBoundP(&mainboardId)).will(returnValue(0));
    size_t size = 0;
    int ret = TopoAddrInfoGetSize(0, &size);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(size, exceptedSize);
}

/**
 * 构造一个PoD中的NPU的地址信息， 并检查是否正确
 */
TEST_F(TopoAddrInfoTest, ut_rootinfo_for_pod)
{
    // mock data
    unsigned int mainboard_id = 0x07;
    char drv_path[256] = "/usr/local/Ascend2";
    UEList ueList;
    memset_s(&ueList, sizeof(UEList), 0x00, sizeof(UEList));
    hex32_to_bin16("000000000000008000100000dfdf1088", ueList.ueList[0].eidList[0].eid.raw);
    hex32_to_bin16("000000000000008000100000dfdf1051", ueList.ueList[0].eidList[1].eid.raw);
    hex32_to_bin16("000000000000008000100000dfdf10b8", ueList.ueList[0].eidList[2].eid.raw);
    hex32_to_bin16("000000000000008000100000dfdf10b0", ueList.ueList[0].eidList[3].eid.raw);
    hex32_to_bin16("000000000000008000100000dfdf10a8", ueList.ueList[0].eidList[4].eid.raw);
    hex32_to_bin16("000000000000008000100000dfdf10a0", ueList.ueList[0].eidList[5].eid.raw);
    hex32_to_bin16("000000000000008000100000dfdf1098", ueList.ueList[0].eidList[6].eid.raw);
    hex32_to_bin16("000000000000008000100000dfdf1090", ueList.ueList[0].eidList[7].eid.raw);
    ueList.ueList[0].eidNum = 8;
    hex32_to_bin16("000000000000004000100000dfdf14d8", ueList.ueList[1].eidList[0].eid.raw);
    ueList.ueList[1].eidNum = 1;
    hex32_to_bin16("000000000000004000100000dfdf10d8", ueList.ueList[2].eidList[0].eid.raw);
    ueList.ueList[2].eidNum = 1;
    ueList.ueNum = 3;

    MOCKER(hal_get_mainboard_id).stubs().with(any(), outBoundP(&mainboard_id)).will(returnValue(0));
    MOCKER(hal_get_driver_install_path).stubs().with(outBoundP(drv_path, strlen(drv_path)), any()).will(returnValue(0));
    MOCKER(HalGetUBEntityList).stubs().with(any(), outBoundP(&ueList)).will(returnValue(0));

    size_t bufSize = 4096;
    char* buf = (char*)malloc(bufSize);
    memset_s(buf, bufSize, 0x00, bufSize);
    int ret = TopoAddrInfoGet(0, buf, &bufSize);
    EXPECT_EQ(ret, 0);
    printf("[%s]\n", buf);
    // 校验PG口EID在地址信息中
    EXPECT_TRUE(strstr(buf, "dfdf14d8") !=  NULL);
    EXPECT_TRUE(strstr(buf, "dfdf10d8") !=  NULL);
    free(buf);
}
