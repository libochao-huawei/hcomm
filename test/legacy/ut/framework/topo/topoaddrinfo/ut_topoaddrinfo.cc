#include "gtest/gtest.h"
#include <time.h>
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <cstdlib>
#include <string>
#include <stdint.h>
#include <ctype.h>
#include <dlfcn.h>
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

int mock_dcmi_init()
{
    // 模拟初始化耗时
    sleep(1);
    return 0;
}

int mock_dcmiv2_get_mainboard_id(int npu_id, unsigned int* mainboard_id)
{
    *mainboard_id = 0x07;
    return 0;
}

int mock_get_logicid_from_chipphy_id(unsigned int phyId, unsigned int* logicId)
{
   *logicId = phyId;
   return 0;
}

void *mock_dlsym(void *handle, const char *symbol)
{
    if (strcmp(symbol, "dcmiv2_init") == 0) {
        return (void*)mock_dcmi_init;
    }
    if (strcmp(symbol, "dcmiv2_get_mainboard_id") == 0) {
        return (void*)mock_dcmiv2_get_mainboard_id;
    }
    if (strcmp(symbol, "dcmiv2_get_dev_id_from_chip_phyid") == 0
     || strcmp(symbol, "dcmiv2_get_dev_id_by_chip_phy_id") == 0) {
        return (void*)mock_get_logicid_from_chipphy_id;
    }
    return (void*)0x1;
}

void *mock_dlopen(const char *filename, int flag)
{
    return (void*)0x1;
}

TEST_F(TopoAddrInfoTest, ut_multi_init)
{
    // mock data
    unsigned int mainBoardId1 = 0;
    unsigned int mainBoardId2 = 0;
    unsigned int expectedMainboardId = 0x07;
    MOCKER(hal_dlopen).stubs().with(any(), any()).will(invoke(mock_dlopen));
    MOCKER(hal_dlsym).stubs().with(any(), any()).will(invoke(mock_dlsym));
    hal_get_mainboard_id(0, &mainBoardId1);
    EXPECT_EQ(mainBoardId1, expectedMainboardId);
    // 连续初始化两次，模拟多线程初始化，第二次进入等待状态
    hal_get_mainboard_id(0, &mainBoardId2);
    EXPECT_EQ(mainBoardId2, expectedMainboardId);
}

TEST_F(TopoAddrInfoTest, ut_rootinfo_for_server)
{
    // mock data for UBX
    unsigned int mainboard_id = 0x25;
    char drv_path[256] = "/usr/local/Ascend2";
    UEList ueList;
    memset_s(&ueList, sizeof(UEList), 0x00, sizeof(UEList));
    hex32_to_bin16("000000000000004000100000dfdf0008", ueList.ueList[0].eidList[0].eid.raw);
    ueList.ueList[0].eidNum = 1;
    hex32_to_bin16("000000000000006000100000dfdf0010", ueList.ueList[1].eidList[0].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0058", ueList.ueList[1].eidList[1].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0048", ueList.ueList[1].eidList[2].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0040", ueList.ueList[1].eidList[3].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0038", ueList.ueList[1].eidList[4].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0030", ueList.ueList[1].eidList[5].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0028", ueList.ueList[1].eidList[6].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0020", ueList.ueList[1].eidList[7].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0018", ueList.ueList[1].eidList[8].eid.raw);
    ueList.ueList[1].eidNum = 9;
    hex32_to_bin16("000000000000004000100000dfdf0088", ueList.ueList[2].eidList[0].eid.raw);
    ueList.ueList[2].eidNum = 1;
    hex32_to_bin16("00000000000000a000100000dfdf0090", ueList.ueList[3].eidList[0].eid.raw);
    hex32_to_bin16("00000000000000a000100000dfdf00d8", ueList.ueList[3].eidList[1].eid.raw);
    hex32_to_bin16("00000000000000a000100000dfdf00c0", ueList.ueList[3].eidList[2].eid.raw);
    hex32_to_bin16("00000000000000a000100000dfdf00b8", ueList.ueList[3].eidList[3].eid.raw);
    hex32_to_bin16("00000000000000a000100000dfdf00b0", ueList.ueList[3].eidList[4].eid.raw);
    hex32_to_bin16("00000000000000a000100000dfdf00a8", ueList.ueList[3].eidList[5].eid.raw);
    hex32_to_bin16("00000000000000a000100000dfdf00a0", ueList.ueList[3].eidList[6].eid.raw);
    hex32_to_bin16("00000000000000a000100000dfdf0098", ueList.ueList[3].eidList[7].eid.raw);
    ueList.ueList[3].eidNum = 8;
    ueList.ueNum = 4;

    struct dcmi_spod_info spinfo;
    spinfo.sdid = 0x00000000;
    spinfo.super_pod_size = 128;
    spinfo.super_pod_id = 1;
    spinfo.server_index = 1;
    spinfo.chassis_id = 0x00000000;
    spinfo.super_pod_type = 0x00000000;

    MOCKER(hal_get_mainboard_id).stubs().with(any(), outBoundP(&mainboard_id)).will(returnValue(0));
    MOCKER(hal_get_driver_install_path).stubs().with(outBoundP(drv_path, strlen(drv_path)), any()).will(returnValue(0));
    MOCKER(HalGetUBEntityList).stubs().with(any(), outBoundP(&ueList)).will(returnValue(0));
    MOCKER(hal_get_spod_info).stubs().with(any(), outBoundP(&spinfo)).will(returnValue(0));

    size_t bufSize = 4096;
    char* buf = (char*)malloc(bufSize);
    memset_s(buf, bufSize, 0x00, bufSize);
    int ret = TopoAddrInfoGet(0, buf, &bufSize);
    EXPECT_EQ(ret, 0);
    printf("[%s]\n", buf);
    // 校验MESH
    EXPECT_TRUE(strstr(buf, "00000000000000a000100000dfdf0090") !=  NULL);
    EXPECT_TRUE(strstr(buf, "00000000000000a000100000dfdf00c0") !=  NULL);
    EXPECT_TRUE(strstr(buf, "00000000000000a000100000dfdf00b8") !=  NULL);
    EXPECT_TRUE(strstr(buf, "00000000000000a000100000dfdf00b0") !=  NULL);
    EXPECT_TRUE(strstr(buf, "00000000000000a000100000dfdf00a8") !=  NULL);
    EXPECT_TRUE(strstr(buf, "00000000000000a000100000dfdf00a0") !=  NULL);
    EXPECT_TRUE(strstr(buf, "00000000000000a000100000dfdf0090") !=  NULL);
    EXPECT_TRUE(strstr(buf, "00000000000000a000100000dfdf00d8") ==  NULL); // mesh中的PG不在其中

    // 校验CLOS
    EXPECT_TRUE(strstr(buf, "000000000000006000100000dfdf0058") !=  NULL); // mesh中的PG不在其中
    // 校验mesh层net type正确
    EXPECT_TRUE(strstr(buf, "TOPO_FILE_DESC") !=  NULL);
    // 校验clos层net type正确
    free(buf);
}

TEST_F(TopoAddrInfoTest, ut_rootinfo_for_ubx)
{
    // mock data for UBX
    unsigned int mainboard_id = 0x44;
    char drv_path[256] = "/usr/local/Ascend2";
    UEList ueList;
    memset_s(&ueList, sizeof(UEList), 0x00, sizeof(UEList));
    hex32_to_bin16("000000000000004000100000dfdf00a8", ueList.ueList[0].eidList[0].eid.raw);
    hex32_to_bin16("000000000000004000100000dfdf00d9", ueList.ueList[0].eidList[1].eid.raw);
    hex32_to_bin16("000000000000004000100000dfdf00c0", ueList.ueList[0].eidList[2].eid.raw);
    hex32_to_bin16("000000000000004000100000dfdf00b8", ueList.ueList[0].eidList[3].eid.raw);
    hex32_to_bin16("000000000000004000100000dfdf00b0", ueList.ueList[0].eidList[4].eid.raw);
    ueList.ueList[0].eidNum = 5;
    hex32_to_bin16("000000000000006000100000dfdf0088", ueList.ueList[1].eidList[0].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf00d8", ueList.ueList[1].eidList[1].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0098", ueList.ueList[1].eidList[2].eid.raw);
    hex32_to_bin16("000000000000006000100000dfdf0090", ueList.ueList[1].eidList[3].eid.raw);
    ueList.ueList[1].eidNum = 4;
    ueList.ueNum = 2;

    struct dcmi_spod_info spinfo;
    spinfo.sdid = 0x00000000;
    spinfo.super_pod_size = 128;
    spinfo.super_pod_id = 1;
    spinfo.server_index = 1;
    spinfo.chassis_id = 0x00000000;
    spinfo.super_pod_type = 0x00000000;

    MOCKER(hal_get_mainboard_id).stubs().with(any(), outBoundP(&mainboard_id)).will(returnValue(0));
    MOCKER(hal_get_driver_install_path).stubs().with(outBoundP(drv_path, strlen(drv_path)), any()).will(returnValue(0));
    MOCKER(HalGetUBEntityList).stubs().with(any(), outBoundP(&ueList)).will(returnValue(0));
    MOCKER(hal_get_spod_info).stubs().with(any(), outBoundP(&spinfo)).will(returnValue(0));

    size_t bufSize = 4096;
    char* buf = (char*)malloc(bufSize);
    memset_s(buf, bufSize, 0x00, bufSize);
    int ret = TopoAddrInfoGet(0, buf, &bufSize);
    EXPECT_EQ(ret, 0);
    printf("[%s]\n", buf);
    // 校验PG口EID在地址信息中
    EXPECT_TRUE(strstr(buf, "000000000000004000100000dfdf00d9") !=  NULL);
    EXPECT_TRUE(strstr(buf, "000000000000006000100000dfdf0098") !=  NULL);
    EXPECT_TRUE(strstr(buf, "000000000000006000100000dfdf0088") !=  NULL);
    EXPECT_TRUE(strstr(buf, "000000000000006000100000dfdf0090") !=  NULL);
    // 校验mesh层net type正确
    EXPECT_TRUE(strstr(buf, "TOPO_FILE_DESC") !=  NULL);
    // 校验clos层net type正确
    free(buf);
}