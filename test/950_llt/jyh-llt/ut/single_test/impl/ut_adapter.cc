#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <driver/ascend_hal.h>
#include <runtime/rt.h>
#include <sys/epoll.h>
#include <chrono>
#include "network/hccp.h"
#include "adapter_rts.h" //FIXME!! why include private .h files
#include "adapter_tdt.h"
#define private public
#define protected public
#include "topoinfo_detect.h"
#include "dispatcher.h"
#include "dispatcher_pub.h"
#include "tbe_vector_reduce.h"
#undef protected
#undef private
#include "externalinput.h"
#include "config.h"
#include <hccl/base.h>
#include <hccl/hccl_types.h>
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub.h"
#include "sal.h"
#include "task_profiling_pub.h"
#include "dlra_function.h"
#include "dlhal_function.h"
#include "dltdt_function.h"
#include <externalinput_pub.h>
#include "task_overflow_pub.h"
#include "profiler_manager.h"
#include "device_capacity.h"
#include "remote_notify.h"
#include "hcom_pub.h"
#include "hccl_network_pub.h"

extern HcclResult CreateCq(RdmaHandle rdmaHandle, CqInfo& cq);
extern HcclResult CreateNormalQp(RdmaHandle rdmaHandle, QpInfo& qp);

using namespace std;
constexpr int32_t INVALID_PAGESIZE = -1;

class RuntimeTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        SetFftsSwitch(false);
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "RuntimeTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "RuntimeTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        DlHalFunction::GetInstance().DlHalFunctionInit();
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
HcclDispatcher RuntimeTest::dispatcherPtr = nullptr;
DispatcherPub *RuntimeTest::dispatcher = nullptr;
class TDTTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "TDTTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TDTTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {

        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};
class HALTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "HALTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HALTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};
#define RELEASE_DEVICE_PTR(dev_ptr)\
    do  \
    {   \
        if (dev_ptr) \
        {   \
            hrtFree(dev_ptr);  \
            dev_ptr = NULL;     \
        }   \
    }while(0)

#define UT_CHECK_NULL_PTR(ptr) do {            \
        if (NULL == ptr) {                          \
            HCCL_ERROR("Pointer[%s] is NULL",#ptr);\
            return;                                \
        }                                           \
    } while(0)

#define UT_EXPECT_EQ_OR_RETURN(ret, value) do { \
        EXPECT_EQ(ret, value);                      \
        if (ret != value) {                         \
            HCCL_ERROR("ret[%d]", ret);             \
            return;                                 \
        }                                           \
    } while(0)

s32 fake_rtGetSocVersion(char *chipVer, const u32 maxLen)
{
    sal_memcpy(chipVer, sizeof("Ascend910"), "Ascend910", sizeof("Ascend910"));
    return DRV_ERROR_NONE;
}

TEST_F(RuntimeTest, ut_rt_get_device_type_err)
{
    s32 ret = HCCL_SUCCESS;

    DevType  chipType;

    MOCKER(rtGetSocVersion)
    .stubs()
    .will(returnValue(1));

    ret  = hrtGetDeviceTypeStub(chipType);
    EXPECT_NE(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER(rtGetSocVersion)
    .stubs()
    .will(invoke(fake_rtGetSocVersion));

    ret  = hrtGetDeviceTypeStub(chipType);
    EXPECT_EQ(ret, HCCL_SUCCESS);/*代码临时适配，芯片类型不匹配是，强制设置为1910*/
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_rt_ra_send_wr)
{
    s32 ret = HCCL_SUCCESS;

    MOCKER(RaSendWr)
    .expects(atMost(1))
    .will(returnValue(0));

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HrtRaSendWr(NULL, NULL, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();

    MOCKER(RaSendWr)
    .expects(atLeast(1))
    .will(returnValue(-2));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(1));

    ret = HrtRaSendWr(NULL, NULL, NULL);
    EXPECT_EQ(ret, HCCL_E_ROCE_TRANSFER);
    GlobalMockObject::verify();

    MOCKER(RaSendWr)
    .stubs()
    .will(returnValue(ROCE_EAGAIN))
    .then(returnValue(0));

    ret = HrtRaSendWr(NULL, NULL, NULL); 
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_ra_rdev_deinit)
{
    s32 ret = HCCL_SUCCESS;
    RdmaHandle rdmaHandle;

    MOCKER(RaRdevDeinit)
    .expects(atMost(1))
    .will(returnValue(-2));

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HrtRaRdmaDeInit(rdmaHandle, 1);
    EXPECT_EQ(ret, 19);
}

TEST_F(RuntimeTest, ut_ra_rdev_deinit1)
{
    s32 ret = HCCL_SUCCESS;
    RdmaHandle rdmaHandle;

    MOCKER(RaRdevDeinit)
    .expects(atMost(1))
    .will(returnValue(-2));

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HrtRaRdmaDeInitRef(rdmaHandle, 1);
}

TEST_F(RuntimeTest, test_hrtRaSocketDeInitRef)
{
    s32 ret = HCCL_SUCCESS;
    SocketHandle socketHandle;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtRaSocketDeInitRef(socketHandle);
}

TEST_F(RuntimeTest, ut_hrtRaTypicalSendWr)
{
    s32 ret = HCCL_SUCCESS;

    MOCKER(RaTypicalSendWr)
    .expects(atMost(1))
    .will(returnValue(0));

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtRaTypicalSendWr(NULL, NULL, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();

    MOCKER(RaTypicalSendWr)
    .expects(atLeast(1))
    .will(returnValue(-2));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(1));

    ret = hrtRaTypicalSendWr(NULL, NULL, NULL);
    EXPECT_EQ(ret, HCCL_E_ROCE_TRANSFER);
    GlobalMockObject::verify();

    MOCKER(RaTypicalSendWr)
    .stubs()
    .will(returnValue(SOCK_EAGAIN))
    .then(returnValue(0));

    ret = hrtRaTypicalSendWr(NULL, NULL, NULL); 
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtRaTypicalQpCreate)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 rdma = 0;
    RdmaHandle rdmaHandle = &rdma;

    struct TypicalQp qpInfo;

    QpHandle qpHandle = nullptr;

    ret = hrtRaTypicalQpCreate(rdmaHandle, QP_FLAG_RC, OPBASE_QP_MODE, &qpInfo, qpHandle);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

TEST_F(RuntimeTest, ut_HrtRaTypicalQpCreate_When_OOM_Expect_ReturnIsHCCL_E_OOM)
{
    MOCKER(RaTypicalQpCreate)
    .stubs()
    .will(returnValue(328100));
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 rdma = 0;
    RdmaHandle rdmaHandle = &rdma;

    struct TypicalQp qpInfo;

    QpHandle qpHandle = nullptr;

    ret = hrtRaTypicalQpCreate(rdmaHandle, QP_FLAG_RC, OPBASE_QP_MODE, &qpInfo, qpHandle);
    EXPECT_EQ(ret, HCCL_E_OOM);
}

TEST_F(RuntimeTest, test_hrtRaTypicalQpModify)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 rdma = 0;
    RdmaHandle rdmaHandle = &rdma;

    struct TypicalQp localQpInfo;
    struct TypicalQp remoteQpInfo;

    QpHandle qpHandle = nullptr;

    ret = hrtRaTypicalQpModify(rdmaHandle, &localQpInfo, &remoteQpInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// Add for optimize runtime test by l on 2018-01-26 Below
// 以下用例，尽力针对单个RTS接口进行测试
// 联调时，涉及rt_set_device的用例，均需要用rtDeviceReset()接口清空信息
TEST_F(RuntimeTest, ut_rt_set_get_device_test)
{
    s32 ret = HCCL_SUCCESS;
    s32 device_count = 8;
    s32 device_id = 0;

    ret =  hrtGetDevice(NULL);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_PTR);

    for (s32 i = 0; i < device_count; i++)
    {
        ret =  hrtSetDevice(i);
        UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

        ret =  hrtGetDevice(&device_id);
        UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

        EXPECT_EQ(i, device_id);
    }

}

TEST_F(RuntimeTest, ut_rt_stream_create_destroy_test)
{
    s32 ret = HCCL_SUCCESS;
    HcclRtStream stream = NULL;

    s32 device_id = 0;
    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    // 申请stream
    ret = rtStreamCreate(&stream, 5);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(stream);

    // 销毁资源
    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_rt_get_device_phyid_and_index)
{
    s32 ret = HCCL_SUCCESS;
    u32 devicePhId = 0;
    u32 deviceLogicId = 0;

    ret =  hrtGetDevicePhyIdByIndex(deviceLogicId, devicePhId);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    ret =  hrtGetDeviceIndexByPhyId(devicePhId, deviceLogicId);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    MOCKER(rtGetDevicePhyIdByIndex)
    .expects(atLeast(1))
    .will(returnValue(1));
    ret =  hrtGetDevicePhyIdByIndex(deviceLogicId, devicePhId);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_RUNTIME);
    GlobalMockObject::verify();
    MOCKER(rtGetDeviceIndexByPhyId)
    .expects(atLeast(1))
    .will(returnValue(1));
    ret =  hrtGetDeviceIndexByPhyId(devicePhId, deviceLogicId);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_RUNTIME);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_rt_get_device_count_test)
{
    s32 ret = HCCL_SUCCESS;
    s32 device_count = 0;

    ret =  hrtGetDeviceCount(NULL);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_PTR);

    ret =  hrtGetDeviceCount(&device_count);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(device_count, 0);

    // 联调时获取环境上实际设备数量进行校验，目前桩函数返回固定值8
    EXPECT_EQ(device_count, 8);
}

TEST_F(RuntimeTest, ut_rt_malloc_setname_free_test)
{
    void* dev_ptr = NULL;
    u64 mem_size = 100;
    s32 ret = HCCL_SUCCESS;

    HCCL_DEBUG("ut_rt_mem_test: mem_size[%d]", mem_size);

    s32 device_id = 0;
    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    // 申请设备内存空间
    ret = hrtMalloc(&dev_ptr, mem_size);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(dev_ptr);

    ret = hrtDevMemAlignWithPage(dev_ptr, mem_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u8 name[HCCL_IPC_MEM_NAME_LEN] = "first_memory test";
    ret = hrtIpcSetMemoryName(dev_ptr, name, mem_size, (u64)sizeof(name));
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(dev_ptr);

    // 释放设备内存空间
    ret = hrtFree(dev_ptr);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_rt_malloc_setnametype_free_test)
{
    void* dev_ptr = NULL;
    u64 mem_size = 100;
    s32 ret = HCCL_SUCCESS;

    HCCL_DEBUG("ut_rt_mem_test: mem_size[%d]", mem_size);

    DevType type610 = DevType::DEV_TYPE_310P1;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(type610))
    .will(returnValue(HCCL_SUCCESS));

    s32 device_id = 0;
    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    // 申请设备内存空间
    ret = hrtMalloc(&dev_ptr, mem_size);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(dev_ptr);

    ret = hrtDevMemAlignWithPage(dev_ptr, mem_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u8 name[HCCL_IPC_MEM_NAME_LEN] = "first_memory test";
    ret = hrtIpcSetMemoryName(dev_ptr, name, mem_size, (u64)sizeof(name));
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(dev_ptr);

    // 释放设备内存空间
    ret = hrtFree(dev_ptr);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_rt_malloc_free_test)
{
    void* dev_ptr = NULL;
    s32 mem_size = 100;

    s32 ret = HCCL_SUCCESS;

    HCCL_DEBUG("ut_rt_mem_test: mem_size[%d]", mem_size);

    s32 device_id = 0;
    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    // 申请设备内存空间
    ret = hrtMalloc(&dev_ptr, mem_size);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(dev_ptr);

    // 释放设备内存空间
    ret = hrtFree(dev_ptr);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    DevType deviceType = DevType::DEV_TYPE_310P3;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    // 申请设备内存空间
    ret = hrtMalloc(&dev_ptr, mem_size, true);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(dev_ptr);

    // 释放设备内存空间
    ret = hrtFree(dev_ptr);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    MOCKER(Is310PDevice)
    .stubs()
    .will(returnValue(true));

    // 申请设备内存空间
    ret = hrtMalloc(&dev_ptr, mem_size);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(dev_ptr);

    // 释放设备内存空间
    ret = hrtFree(dev_ptr);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    // 申请设备内存空间
    ret = hrtMalloc(&dev_ptr, mem_size, true);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(dev_ptr);

    // 释放设备内存空间
    ret = hrtFree(dev_ptr);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
/*
TEST_F(RuntimeTest, ut_drv_device_get_IOtype_test)
{
    s32 ret = HCCL_SUCCESS;
    s32 peer_to_peer_enable;
    s32 device_count = 0;
    s32 device_id = 0;

    ret =  hrtGetDeviceCount(&device_count);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    EXPECT_NE(device_count, 0);

    // 检查入参
    ret =  rt_get_device_peer_access_ability(device_id, NULL);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_PTR);

    // 检查devId的最大值
    device_id = 64;
    ret =  rt_get_device_peer_access_ability(device_id, &peer_to_peer_enable);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_PARA);

    // 检查devId的最小值
    device_id = -2;
    ret =  rt_get_device_peer_access_ability(device_id, &peer_to_peer_enable);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_PARA);

    for (s32 i = 0; i < device_count; i++)
    {
        ret = rt_get_device_peer_access_ability(i, &peer_to_peer_enable);
        UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    }
}

TEST_F(RuntimeTest, ut_drv_device_get_topology_test)
{
    s32 ret = HCCL_SUCCESS;
    s32 peer_to_peer_enable;
    s32 device_count = 0;
    s32 device_id = 0;
    s32* device_list = NULL;

    ret =  hrtGetDeviceCount(&device_count);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    EXPECT_NE(device_count, 0);

    device_list = (s32*) sal_malloc(sizeof(s32) * device_count);
    EXPECT_NE(device_list, (void*)NULL);
    if (device_list == NULL)
    { goto destroy; }

    // 检查入参
    ret = rt_get_device_peer_access_device_list(device_id, NULL);
    EXPECT_EQ(ret, HCCL_E_PTR);
    if (ret != HCCL_E_PTR)
    { goto destroy; }

    // 检查devId的最大值
    device_id = 64;
    ret = rt_get_device_peer_access_device_list(device_id, device_list);
    EXPECT_EQ(ret, HCCL_E_PARA);
    if (ret != HCCL_E_PARA)
    { goto destroy; }

    // 检查devId的最小值
    device_id = -2;
    ret = rt_get_device_peer_access_device_list(device_id, device_list);
    EXPECT_EQ(ret, HCCL_E_PARA);
    if (ret != HCCL_E_PARA)
    { goto destroy; }

    for (s32 i = 0; i < device_count; i++)
    {
        s32 exp_ret;
        ret = rt_get_device_peer_access_ability(i, &peer_to_peer_enable);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        if (ret != HCCL_SUCCESS)
        { goto destroy; }

        ret = rt_get_device_peer_access_device_list(i, device_list);
        if (peer_to_peer_enable)
        {
            exp_ret = HCCL_SUCCESS;
        }
        else
        {
            exp_ret = HCCL_E_PARA;
        }
        EXPECT_EQ(exp_ret, ret);
        if (exp_ret != ret)
        {
            goto destroy;
        }

    }

destroy:
    HCCL_RELEASE_PTR_AND_SET_NULL(device_list);
}


TEST_F(RuntimeTest, ut_drv_device_get_pcieinfo_test)
{
    s32 ret = HCCL_SUCCESS;
    s32 bus_id, dev_id, fun_id;
    s32 device_count = 0;
    s32 device_id = 0;

    // 检查入参
    ret = rt_get_device_pci_info(device_id, NULL, NULL, NULL);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_PTR);

    // 检查devId的最大值
    device_id = 64;
    ret = rt_get_device_pci_info(device_id, &bus_id, &dev_id, &fun_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_DRV);

    // 检查devId的最小值
    device_id = -2;
    ret = rt_get_device_pci_info(device_id, &bus_id, &dev_id, &fun_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_DRV);

    ret =  hrtGetDeviceCount(&device_count);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    EXPECT_NE(device_count, 0);
    if (device_count != 0)
    { return; }

    for (s32 i = 0; i < device_count; i++)
    {
        ret = rt_get_device_pci_info(i, &bus_id, &dev_id, &fun_id);
        UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

        // 联调时获取设备的PCIE编号进行校验，目前桩函数暂时用devId表示bus id
        UT_EXPECT_EQ_OR_RETURN(bus_id, i);
    }
}
// Add for optimize runtime test by l on 2018-01-26 Above
*/

TEST_F(RuntimeTest, ut_rt_get_device_control)
{
    HcclResult ret = HCCL_SUCCESS;
    s32 device_count = 0;
    s32 device_id = 0;

    ret =  hrtGetDeviceCount(NULL);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_PTR);

    ret =  hrtGetDevice(NULL);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_E_PTR);

    ret =  hrtGetDeviceCount(&device_count);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    EXPECT_NE(device_count, 0);

    for (s32 i = 0; i < device_count; i++)
    {
        ret =  hrtSetDevice(i);
        UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

        ret =  hrtGetDevice(&device_id);
        UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

        EXPECT_EQ(device_id, i);
    }
}

/* 设备内存UT测试用例 */
TEST_F(RuntimeTest, ut_rt_mem_test)
{
    void* dev_ptr = NULL;
    s32     mem_size = 100;

    s32 ret = HCCL_SUCCESS;
    s32    device_id = 0;

    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    HCCL_DEBUG("ut_rt_mem_test: mem_size[%d]", mem_size);

    /* 申请设备内存空间 */
    ret = hrtMalloc(&dev_ptr, mem_size);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(dev_ptr);

    /* 释放设备内存空间 */
    ret = hrtFree(dev_ptr);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
}

/* 设备内存拷贝测试用例 */
TEST_F(RuntimeTest, ut_rt_mem_copy_test)
{
    void* dev_src_ptr = NULL;
    void* dev_dst_ptr = NULL;
    char* src_ptr = NULL;
    char* dst_ptr = NULL;
    char     src_value = 0xAA;
    s32     mem_size = 100;
    HcclRtStream  stream;
    s32 ret = HCCL_SUCCESS;
    s32    device_id = 0;

    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    HCCL_DEBUG("ut_rt_mem_cpy_test: mem_size[%d]", mem_size);

    ret = rtStreamCreate(&stream, 5);
    Stream hcclStream(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    if (RT_ERROR_NONE != ret)
    { goto destroy; }

    /* 申请源设备内存空间 */
    ret = hrtMalloc(&dev_src_ptr, mem_size);
    EXPECT_NE((long)dev_src_ptr, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (NULL == dev_src_ptr)
    { goto destroy; }

    /* 申请目的设备内存空间 */
    ret = hrtMalloc(&dev_dst_ptr, mem_size);
    EXPECT_NE((long)dev_dst_ptr, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (NULL == dev_dst_ptr)
    {
        goto destroy;
    }

    /* 申请源Host内存空间 */
    src_ptr = (char*)sal_malloc(mem_size);
    EXPECT_NE((long)src_ptr, NULL);
    if (NULL == src_ptr)
    {
        goto destroy;
    }

    /* 申请目的Host内存空间 */
    dst_ptr = (char*)sal_malloc(mem_size);
    EXPECT_NE((long)dst_ptr, NULL);
    if (NULL == dst_ptr)
    {
        goto destroy;
    }

    /* 源内存初始化 */
    ret = sal_memset(src_ptr, mem_size, src_value, mem_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (HCCL_SUCCESS != ret)
    {
        goto destroy;
    }

    /* 校验源内存初始化结果 */
    EXPECT_EQ(*src_ptr, src_value);
    EXPECT_EQ(*(src_ptr + mem_size - 1), src_value);

    /* 清空Host目的内存 */
    ret = sal_memset(dst_ptr, mem_size, 0, mem_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (HCCL_SUCCESS != ret)
    {
        goto destroy;
    }


    ret = dispatcher->MemcpyAsync(dst_ptr, mem_size,src_ptr, 0,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_HOST, hcclStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->MemcpyAsync(dst_ptr, mem_size,src_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_HOST, hcclStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    /* 校验内存拷贝结果 */
    EXPECT_EQ(*dst_ptr, src_value);
    EXPECT_EQ(*(dst_ptr + mem_size - 1), src_value);


    /* 清空Host目的内存 */
    ret = sal_memset(dst_ptr, mem_size, 0, mem_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (HCCL_SUCCESS != ret)
    {
        goto destroy;
    }
    ret = dispatcher->MemcpyAsync(dev_src_ptr,mem_size, src_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE, hcclStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->MemcpyAsync(dev_dst_ptr,mem_size, dev_src_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_DEVICE, hcclStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->MemcpyAsync(dst_ptr, mem_size,dev_dst_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST, hcclStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    /* 校验内存拷贝结果 */
    EXPECT_EQ(*dst_ptr, src_value);
    EXPECT_EQ(*(dst_ptr + mem_size - 1), src_value);

    /* 异常参数覆盖 */
    ret = dispatcher->MemcpyAsync(dst_ptr, mem_size,src_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_RESERVED, hcclStream);
    EXPECT_NE(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->MemcpyAsync(NULL,mem_size, NULL, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_HOST, hcclStream);
    EXPECT_NE(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }
destroy:
    /* 释放内存空间 */
    HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
    HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
    RELEASE_DEVICE_PTR(dev_src_ptr);
    RELEASE_DEVICE_PTR(dev_dst_ptr);
    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);
}

typedef struct tag_ut_rt_event_test
{
    HcclRtEvent     event;
    HcclRtStream    stream;
    s32 device_id;
} ut_rt_event_test_t;

/* event 测试线程 */
void* test_task_for_rt_event_test(void* p)
{
    ut_rt_event_test_t* sync_event_ptr =  (ut_rt_event_test_t*) p;
    s32 ret = HCCL_SUCCESS;

    HCCL_INFO("new thread running");
    HCCL_INFO("waitting event");

    ret =  hrtSetDevice(sync_event_ptr->device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { return (NULL); }
    
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, sync_event_ptr->device_id, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    ret = dispatcher->SignalWait(sync_event_ptr->event, sync_event_ptr->stream, 1, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if ( HCCL_SUCCESS == ret )
    { HCCL_INFO("take event success"); }
    else
    { return (NULL); }

    ret = rtStreamSynchronize(sync_event_ptr->stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    if ( RT_ERROR_NONE == ret )
    { HCCL_INFO("Synchronize success"); }
    else
    { return (NULL); }

    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
    HCCL_INFO("thread exit");
    return (NULL);
}

TEST_F(RuntimeTest, ut_rt_reduce_char)
{

    s32 ret = HCCL_SUCCESS;
    HcclRtStream rtStream;
    s8 src1[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    s8 src2[10] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
    s8 dst[10];
    u32 count = 10;
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp red_op = HCCL_REDUCE_SUM;
    s32    device_id = 0;

    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    /* 申请stream */
    ret = rtStreamCreate(&rtStream, 5);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    Stream stream(rtStream);

    //异常分支处理
    ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, red_op, stream, dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, HCCL_REDUCE_RESERVED, stream, dst);
    EXPECT_EQ(ret, HCCL_E_PARA);


    ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, red_op, stream, dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, red_op, stream, dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = dispatcher->ReduceAsync(src1, src2, count, datatype, red_op, stream, HcclReduceType::HCCL_TBE_REDUCE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (ret !=  HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(rtStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret !=  HCCL_SUCCESS)
    { goto destroy; }
    /* 销毁资源 */
destroy:
    if (rtStream != NULL)
    {
        ret = rtStreamDestroy(rtStream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}

TEST_F(RuntimeTest, ut_rt_reduce_float)
{

    s32 ret = HCCL_SUCCESS;
    HcclRtStream rtStream;
    float src1[10] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    float src2[10] = {3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0};
    float dst[10];
    u32 count = 10;
    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_FP16;
    HcclReduceOp red_op = HCCL_REDUCE_SUM;
    s32    device_id = 0;

    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    /* 申请stream */
    ret = rtStreamCreate(&rtStream, 5);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    Stream stream(rtStream);

    ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, red_op, stream, dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret !=  HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(rtStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret !=  HCCL_SUCCESS)
    { goto destroy; }
    /* 销毁资源 */
destroy:
    if (rtStream != NULL)
    {
        ret = rtStreamDestroy(rtStream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}

TEST_F(RuntimeTest, ut_rt_vector_reduce_op)
{

    s32 ret = HCCL_SUCCESS;
    HcclRtStream rtStream = NULL;
    float src1[10] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    float src2[10] = {3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0};
    float dst[10];
    u32 count = 10;
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32    device_id = 0;

    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    /* 申请stream */
    ret = rtStreamCreate(&rtStream, 5);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    Stream stream(rtStream);
    ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, HCCL_REDUCE_PROD, stream, dst);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, HCCL_REDUCE_MAX, stream, dst);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, HCCL_REDUCE_MIN, stream, dst);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(rtStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    // 销毁资源
destroy:
    if (stream != NULL)
    {
        ret = rtStreamDestroy(rtStream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}

TEST_F(RuntimeTest, ut_rt_get_set_Device_v81)
{
    s32 ret = HCCL_SUCCESS;

    set_chip_type_stub(0, static_cast<s32>(DevType::DEV_TYPE_910B));

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 deviceLogicId = 0;
    ret = hrtGetDevice(&deviceLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    set_chip_type_stub(0, static_cast<s32>(DevType::DEV_TYPE_910));
}

TEST_F(RuntimeTest, ut_rt_get_device_type)
{
    s32 ret = HCCL_SUCCESS;

    DevType  chipType;
    s8 machinChipVer[CHIP_VERSION_MAX_LEN] = {0};
    set_chip_type_stub(0, static_cast<s32>(DevType::DEV_TYPE_910));
    ret  = hrtGetSocVer(machinChipVer, CHIP_VERSION_MAX_LEN);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret  = strcmp((const char*)machinChipVer, "Ascend910");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret  = hrtGetDeviceTypeStub(chipType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    set_chip_type_stub(1, static_cast<s32>(DevType::DEV_TYPE_910));
    ret  = hrtGetSocVer(machinChipVer, CHIP_VERSION_MAX_LEN);
    set_chip_type_stub(1, static_cast<s32>(DevType::DEV_TYPE_910));

    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret  = strcmp((const char*)machinChipVer, "Ascend910");
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

#ifdef __cplusplus
extern "C"
{
#endif
extern HcclResult __rt_get_dev_ip(s32 chipType, s32 dev_id, u32* ipAddr);

#ifdef __cplusplus
}
#endif

TEST_F(RuntimeTest, ut_rt_get_dev_ip)
{
    HcclResult ret;
    u32 ipAddr = 0;
    u32 check_ip= 0;

    ret = __rt_get_dev_ip(0, 0, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("127.0.0.1");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(0, 1, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("127.0.0.2");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(0, 2, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("127.0.0.3");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(0, 3, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("127.0.0.4");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(0, 4, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("127.0.0.1");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(0, 5, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("127.0.0.2");
    EXPECT_EQ(ipAddr, check_ip);


    ret = __rt_get_dev_ip(0, 6, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("127.0.0.3");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(0, 7, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("127.0.0.4");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(1, 0, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("192.168.1.199");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(1, 1, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("192.168.2.198");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(1, 2, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("192.168.3.197");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(1, 3, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("192.168.4.196");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(1, 4, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("192.168.1.195");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(1, 5, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("192.168.2.194");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(1, 6, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("192.168.3.193");
    EXPECT_EQ(ipAddr, check_ip);

    ret = __rt_get_dev_ip(1, 7, &ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    check_ip = inet_addr("192.168.4.192");
    EXPECT_EQ(ipAddr, check_ip);


    ret = __rt_get_dev_ip(2, 7, &ipAddr);
    EXPECT_NE(ret, HCCL_SUCCESS);

    ret = __rt_get_dev_ip(0, 0, NULL);
    EXPECT_NE(ret, HCCL_SUCCESS);

}

TEST_F(RuntimeTest, ut_rt_inline_vector_reduce_test)
{
    s32 ret = HCCL_SUCCESS;
    HcclRtStream rtStream;
    Stream stream;
    s32 ret_for_result = HCCL_SUCCESS;

    char* src1_char = NULL;
    char* dst_char = NULL;
    float* src1_float = NULL;
    float* dst_float = NULL;
    char tempdst_char;
    float tempdst_float;
    u32 count = 10;

    s32 device_id = 0;
    ret =  hrtSetDevice(device_id);

    // 申请内存空间(char)
    ret = hrtMalloc((void**)&src1_char, 10);
    EXPECT_NE((long)src1_char, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (NULL == src1_char)
    {
        goto destroy;
    }

    memset(src1_char, 1, 10);

    ret = hrtMalloc((void**)&dst_char, 10);
    EXPECT_NE((long)dst_char, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (NULL == dst_char)
    {
        goto destroy;
    }

    memset(dst_char, 0, 10);

    // 申请内存空间(float)
    ret = hrtMalloc((void**)&src1_float, 40);
    EXPECT_NE((long)src1_float, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (NULL == src1_float)
    {
        goto destroy;
    }

    ret = hrtMalloc((void**)&dst_float, 40);
    EXPECT_NE((long)dst_float, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (NULL == dst_float)
    {
        goto destroy;
    }

    for (s32 i = 0; i < count; i++)
    {
        src1_float[i] = 1.0;
        dst_float[i] = 2.0;
    }

    // 申请stream
    ret = rtStreamCreate(&rtStream, 5);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    stream = Stream(rtStream);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    // inline  reduce不支持类型检测
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_INT8, HcclReduceOp::HCCL_REDUCE_SUM, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_INT8, HcclReduceOp::HCCL_REDUCE_PROD, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_INT8, HcclReduceOp::HCCL_REDUCE_MAX, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_INT8, HcclReduceOp::HCCL_REDUCE_MIN, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);

    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_INT32, HcclReduceOp::HCCL_REDUCE_SUM, stream, (void*)dst_char);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_INT32, HcclReduceOp::HCCL_REDUCE_PROD, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_INT32, HcclReduceOp::HCCL_REDUCE_MAX, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_INT32, HcclReduceOp::HCCL_REDUCE_MIN, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);

    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_FP16, HcclReduceOp::HCCL_REDUCE_PROD, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_FP16, HcclReduceOp::HCCL_REDUCE_MAX, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_FP16, HcclReduceOp::HCCL_REDUCE_MIN, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);


    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_PROD, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_MAX, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_MIN, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = dispatcher->InlineReduceAsync((void*)src1_char, count, HcclDataType::HCCL_DATA_TYPE_RESERVED, HcclReduceOp::HCCL_REDUCE_RESERVED, stream, (void*)dst_char);
    EXPECT_NE(ret, HCCL_SUCCESS);

    // reduce_float op_sum
    ret = dispatcher->InlineReduceAsync((void*)src1_float, count*sizeof(float), HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_SUM, stream, (void*)dst_float);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // reduce容错检查
    ret = dispatcher->InlineReduceAsync((void*)src1_float, 0, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_SUM, stream, (void*)dst_float);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(rtStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    for (s32 i = 0; i < count; i++)
    {
        tempdst_float = dst_float[i];

        if (tempdst_float != 3.0)
        {
            ret_for_result = HCCL_E_INTERNAL;
            break;
        }
    }

    EXPECT_EQ(ret_for_result, HCCL_SUCCESS);

    if (ret_for_result != HCCL_SUCCESS)
    { goto destroy; }
    // 释放资源
destroy:
    if (src1_char != NULL)
    {
        ret = hrtFree((void*)src1_char);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    if (dst_char != NULL)
    {
        ret = hrtFree((void*)dst_char);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    if (src1_float != NULL)
    {
        ret = hrtFree((void*)src1_float);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    if (dst_float != NULL)
    {
        ret = hrtFree((void*)dst_float);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    if (rtStream != NULL)
    {
        // 销毁资源
        ret = rtStreamDestroy(rtStream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}

// Dispatcher覆盖率补充
TEST_F(RuntimeTest, Dispatcher_destructor_D0)
{
    s32 ret = HCCL_SUCCESS;
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void *dispatcherTestPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherTestPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherTestPtr, nullptr);
    DispatcherPub * dispatcherTest = reinterpret_cast<DispatcherPub*>(dispatcherTestPtr);

    MOCKER(hrtResetDevice)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_PARA));

    if (dispatcherTestPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherTestPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherTestPtr = nullptr;
    }
}

TEST_F(RuntimeTest, dispatcher_test)
{
    s32 ret = HCCL_SUCCESS;
    s32 devid = 0;
    s32 new_devid = 0;
    s32 devtype = 0;
    HcclRtEvent event = NULL;
    HcclRtStream stream = NULL;
    float src1[10] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    float src2[10] = {3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0};
    float dst[10];
    u32 count = 10;
    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_FP32;
    HcclReduceOp red_op = HcclReduceOp::HCCL_REDUCE_SUM;

    hccl::DeviceMem dev_mem_dst = hccl::DeviceMem::alloc(1);
    hccl::DeviceMem dev_mem_src = hccl::DeviceMem::alloc(1);
    hccl::HostMem host_mem_dst = hccl::HostMem::alloc(1);
    hccl::HostMem host_mem_src = hccl::HostMem::alloc(1);
    hccl::Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    
    void *dispatcherTestPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherTestPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherTestPtr, nullptr);
    DispatcherPub * dispatcherTest = reinterpret_cast<DispatcherPub*>(dispatcherTestPtr);

    // 销毁资源
destroy:
    if (stream != NULL)
    {
        ret = rtStreamDestroy(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    if (dispatcherTestPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherTestPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherTestPtr = nullptr;
    }

}

TEST_F(RuntimeTest, DispatcherEnhanced_test)
{
    s32 ret = HCCL_SUCCESS;
    s32 devid = 0;

    HcclRtStream rtStream = NULL;
    float src1[10] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    float src2[10] = {3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0};
    float dst[10];
    u32 count = 10;
    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_FP32;
    HcclReduceOp red_op = HcclReduceOp::HCCL_REDUCE_SUM;


    u8 test_uniqueid[HCCL_IPC_MEM_NAME_LEN] = "notify-open-test";

    MOCKER_CPP(&TbeReduce::TbeVectorReduce::Run)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TbeReduce::TbeVectorReduce::SetGlobalWorkSpace)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    hccl::DeviceMem dev_mem_dst = hccl::DeviceMem::alloc(1);
    hccl::DeviceMem dev_mem_src = hccl::DeviceMem::alloc(1);
    hccl::HostMem host_mem_dst = hccl::HostMem::alloc(1);
    hccl::HostMem host_mem_src = hccl::HostMem::alloc(1);
    hccl::Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    u32 pid = 0;
    ret = SalGetBareTgid(&pid);    // 当前进程id
    EXPECT_EQ(ret, HCCL_SUCCESS);
    int maxLen = HCCL_IPC_MEM_NAME_LEN;

    // 创建信号
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string tag1 = "test_signal_create_1";
    ret = notifyPool->RegisterOp(tag1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
    std::shared_ptr<RemoteNotify> remoteNotify = nullptr;
    RemoteRankInfo info(0, 0, pid);
    ret = notifyPool->Alloc(tag1, info, localNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(localNotify, nullptr);

    std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
    ret = localNotify->Serialize(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remoteNotify.reset(new (std::nothrow) RemoteNotify());
    EXPECT_NE(remoteNotify, nullptr);

    ret = remoteNotify->Init(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = remoteNotify->Open();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = notifyPool->UnregisterOp(tag1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = dispatcher->MemcpyAsync(host_mem_dst, host_mem_src, stream1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = dispatcher->TbeReduceAsync(dev_mem_src.ptr(), dev_mem_src.ptr(), count, datatype, HCCL_REDUCE_MAX, stream1, dev_mem_dst.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = dispatcher->InlineReduceAsync(dev_mem_src.ptr(), count, HcclDataType::HCCL_DATA_TYPE_FP32,
        HcclReduceOp::HCCL_REDUCE_SUM, stream1, dev_mem_dst.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rtStreamCreate(&rtStream, 5);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    Stream stream(rtStream);

    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = remoteNotify->Post(stream, dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = localNotify->Wait(stream, dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->MemcpyAsync(dst,sizeof(dst), src1, 0, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_HOST, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->MemcpyAsync(dst,sizeof(dst), src1, count, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_HOST, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->MemcpyAsync(dst, sizeof(dst),src1, count, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_RESERVED, stream);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
    if (ret != HCCL_E_RUNTIME)
    { goto destroy; }

    ret = dispatcher->TbeReduceAsync((void*)src1, (void*)src2, 0, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_SUM, stream, (void*)dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->TbeReduceAsync((void*)src1, (void*)src2, count, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_SUM, stream, (void*)dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->TbeReduceAsync((void*)src1, (void*)src2, count, HcclDataType::HCCL_DATA_TYPE_INT8, HcclReduceOp::HCCL_REDUCE_PROD, stream, (void*)dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->TbeReduceAsync((void*)src1, (void*)src2, count, HcclDataType::HCCL_DATA_TYPE_FP16, HcclReduceOp::HCCL_REDUCE_MAX, stream, (void*)dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->TbeReduceAsync((void*)src1, (void*)src2, count, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_MIN, stream, (void*)dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->TbeReduceAsync((void*)src1, (void*)src2, count, HcclDataType::HCCL_DATA_TYPE_RESERVED, HcclReduceOp::HCCL_REDUCE_RESERVED, stream, (void*)dst);
    EXPECT_EQ(ret, HCCL_E_PARA);
    if (ret != HCCL_E_PARA)
    { goto destroy; }

    ret = dispatcher->TbeReduceAsync((void*)src1, (void*)src2, count, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_RESERVED, stream, (void*)dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_E_PARA)
    { goto destroy; }

    ret = dispatcher->InlineReduceAsync((void*)src1, count, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_SUM, stream, (void*)dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(rtStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (ret != HCCL_SUCCESS)
    { goto destroy; }


    // 销毁资源
destroy:
    localNotify = nullptr;
    remoteNotify = nullptr;

    if (rtStream != NULL)
    {
        ret = rtStreamDestroy(rtStream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}

TEST_F(RuntimeTest, drv_disableP2P_test)
{
    u32 devid = 0;
    HcclResult ret = hrtDisableP2P(devid, devid);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, rt_ra_socket_send)
{
    HcclResult ret = HCCL_SUCCESS;
    s32 devid = 0;
    void *socketHandle = (void*)&devid;
    char uHandle = 0;
    char data[1];
    data[0] = '1';

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 size =  0x8FFFFFFFFFFFFFFF;
    ret = hrtRaSocketBlockSend(&uHandle, data, size);
    EXPECT_EQ(ret, HCCL_E_PARA);

    u64 sendlen = 1;
    MOCKER(RaSocketSend)
    .stubs()
    .with(any(), any(), any(), outBoundP(&sendlen))
    .will(returnValue(0))
    .then(returnValue(128201))
    .then(returnValue(1));
    ret = hrtRaSocketBlockSend(&uHandle, data, sendlen);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    auto time_point = std::chrono::steady_clock::now(); 
    std::chrono::steady_clock::duration duration = std::chrono::seconds(999999); 
    // 将time_point与duration相加 
    auto new_time_point = time_point + duration;
    MOCKER(GetExternalInputHcclLinkTimeOut)
        .stubs()
        .will(returnValue(0));
    MOCKER((std::chrono::steady_clock::now))
        .stubs()
        .will(returnValue(time_point))
        .then(returnValue(new_time_point));
    ret = hrtRaSocketBlockSend(&uHandle, data, sendlen);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);

    ret = hrtRaSocketBlockSend(&uHandle, data, sendlen);
    EXPECT_EQ(ret, HCCL_E_NETWORK);

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_WaitTopoExchangeServerCompelte)
{
    HcclResult ret = HCCL_SUCCESS;
    std::shared_ptr<TopoInfoDetect> topoDetectAgent = std::make_shared<TopoInfoDetect>();

    auto time_point = std::chrono::steady_clock::now(); 
    std::chrono::steady_clock::duration duration = std::chrono::seconds(999999); 
    // 将time_point与duration相加 
    auto new_time_point = time_point + duration;

    MOCKER(GetExternalInputHcclLinkTimeOut)
        .stubs()
        .will(returnValue(0));
    MOCKER((std::chrono::steady_clock::now))
        .stubs()
        .will(returnValue(time_point))
        .then(returnValue(new_time_point));

    topoDetectAgent->g_topoExchangeServerStatus_.EmplaceAndUpdate(0, [] (volatile u32 &status) {
        status = 1;
    });
    ret = topoDetectAgent->WaitTopoExchangeServerCompelte(0);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
    
    GlobalMockObject::verify();
}


TEST_F(RuntimeTest, rt_ra_qp_destroy_timeout)
{
    HcclResult ret;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *handle = nullptr;
    MOCKER(RaQpDestroy)
    .stubs()
    .will(returnValue(-EAGAIN));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(1));

    ret = HrtRaQpDestroy(handle);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();

    void *handle1 = nullptr;
    MOCKER(RaQpDestroy)
    .stubs()
    .will(returnValue(ROCE_EAGAIN))
    .then(returnValue(0));

    ret = HrtRaQpDestroy(handle1); 
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, rt_ra_get_qp_depth)
{
    HcclResult ret;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    int rdma;
    RdmaHandle rdmaHandle = &rdma;
    unsigned int tempDepth;
    unsigned int qpNum;

    ret = HrtRaGetQpDepth(rdmaHandle, &tempDepth, &tempDepth);

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, rt_ra_set_qp_depth)
{
    HcclResult ret;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    int rdma;
    RdmaHandle rdmaHandle = &rdma;
    unsigned int tempDepth{};
    unsigned int qpNum;

    ret = HrtRaSetQpDepth(rdmaHandle, tempDepth, &tempDepth);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, rt_ra_qp_connect_async_timeout)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *handle = nullptr;
    void *sockHandle = nullptr;
    MOCKER(RaQpConnectAsync)
    .stubs()
    .will(returnValue(-EAGAIN));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(1));

    ret = HrtRaQpConnectAsync(handle, sockHandle);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, rt_ra_send_wr_timeout)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void *handle = nullptr;
    struct SendWr wr;
    struct SendWrRsp opRsp = {0};
    u32 wqeIndex;
    MOCKER(RaSendWr)
    .stubs()
    .will(returnValue(-EAGAIN));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(1));

    ret = HrtRaSendWr(handle, &wr, &opRsp);
    EXPECT_EQ(ret, HCCL_E_ROCE_TRANSFER);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, rt_ra_get_notify_base_addr_timeout)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void *handle_;
    u64 notifyBaseVa = 0;  // notify寄存器虚拟地址
    u64 notifyTotalSize = 0;
    MOCKER(RaGetNotifyBaseAddr)
    .stubs()
    .will(returnValue(-EAGAIN));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(1));

    ret = HrtRaGetNotifyBaseAddr(handle_, &notifyBaseVa, &notifyTotalSize);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_ra_init_timeout)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(RaInit)
    .stubs()
    .will(returnValue(HCCP_EAGAIN));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(0));

    RaInitConfig raConfig;
    raConfig.phyId = 0;
    raConfig.nicPosition = 0;
    ret = HrtRaInit(&raConfig);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
    GlobalMockObject::verify();

    MOCKER(RaInit)
    .stubs()
    .will(returnValue(328002));
    struct RaInitConfig config;
    ret = HrtRaInit(&config);
    EXPECT_EQ(ret, HCCL_E_PARA);

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, rt_ra_deinit_timeout)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(RaDeinit)
    .stubs()
    .will(returnValue(-EAGAIN));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(1));

    RaInitConfig raConfig;
    raConfig.phyId = 0;
    raConfig.nicPosition = 0;
    ret = HrtRaDeInit(&raConfig);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, rt_ra_socket_listen_start_timeout)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(RaSocketListenStart)
    .stubs()
    .will(returnValue(-EAGAIN));


    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(1));

    SocketListenInfoT sockListen;
    ret = hrtRaSocketListenStart(&sockListen, 1);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    GlobalMockObject::verify();

    MOCKER(RaSocketListenStart)
    .stubs()
    .will(returnValue(128205));
    SocketListenInfoT sockListen1;
    ret = hrtRaSocketNonBlockListenStart(&sockListen1, 1);
    EXPECT_EQ(ret, HCCL_E_UNAVAIL);

    GlobalMockObject::verify();

    MOCKER(hrtRaSocketNonBlockListenStart)
    .stubs()
    .will(returnValue(HCCL_E_AGAIN))
    .then(returnValue(0));

    SocketListenInfoT sockListen2;
    ret = hrtRaSocketListenStart(&sockListen2, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_HrtDevFree)
{
    void *data = malloc(10);

    HcclResult ret = HrtDevFree(data);
    EXPECT_EQ(ret, HCCL_E_PTR);

    free(data);
}

TEST_F(RuntimeTest, test_HrtDevMallocAndFree)
{
    HcclResult ret;
    u64 size = 4096;
    void *devMemAddr{ nullptr };
    ret = HrtDevMalloc(&devMemAddr, size);
    EXPECT_EQ(ret, 2);

    ret = HrtDevFree(devMemAddr);
    EXPECT_EQ(ret, 2);
}

TEST_F(RuntimeTest, rt_ra_socket_listen_stop_timeout)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(RaSocketListenStop)
    .stubs()
    .will(returnValue(SOCK_EAGAIN));


    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(0));

    SocketListenInfoT sockListen;
    ret = hrtRaSocketListenStop(&sockListen, 1);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, rt_ra_socket_listen_stop_nodev)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(RaSocketListenStop)
    .stubs()
    .will(returnValue(SOCK_ENODEV));

    SocketListenInfoT sockListen;
    ret = hrtRaSocketListenStop(&sockListen, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, rt_ra_socket_batch_connect_timeout)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(RaSocketBatchConnect)
    .stubs()
    .will(returnValue(-EAGAIN));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(1));

    SocketConnectInfoT sockConn;
    ret = hrtRaSocketBatchConnect(&sockConn, 1, 1);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    GlobalMockObject::verify();

    MOCKER(RaSocketBatchConnect)
    .stubs()
    .will(returnValue(SOCK_EAGAIN))
    .then(returnValue(0));

    SocketConnectInfoT sockConn1;
    ret = hrtRaSocketBatchConnect(&sockConn1, 1, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, rt_ra_socket_batch_close_timeout)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(RaSocketBatchClose)
    .stubs()
    .will(returnValue(SOCK_EAGAIN));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .expects(once())
    .will(returnValue(0));

    SocketCloseInfoT sockConn;
    ret = hrtRaSocketBatchClose(&sockConn, 1, 1);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
    GlobalMockObject::verify();
}

s32 fake_rtGetSocVersionPremiumA(char *chipVer, const u32 maxLen)
{
    sal_memcpy(chipVer, sizeof("Ascend910PremiumA"), "Ascend910PremiumA", sizeof("Ascend910PremiumA"));
    return DRV_ERROR_NONE;
}

s32 fake_rtGetSocVersionProA(char *chipVer, const u32 maxLen)
{
    sal_memcpy(chipVer, sizeof("Ascend910ProA"), "Ascend910ProA", sizeof("Ascend910ProA"));
    return DRV_ERROR_NONE;
}

TEST_F(RuntimeTest, rtgetsocver_test)
{
    u32 ret = 0;

    s8 machChipVer[CHIP_VERSION_MAX_LEN] = {0};
    hrtGetSocVer(machChipVer, CHIP_VERSION_MAX_LEN);
    ret = strcmp((const char*)machChipVer, "Ascend910");
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TDTTest, tsdOpen_test)
{
    HcclResult ret;
    ret = hrtOpenTsd(1,2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(TsdOpen)
    .stubs()
    .will(returnValue(2));
    ret = hrtOpenTsd(1,2);
    EXPECT_EQ(ret, HCCL_E_UNAVAIL);
    TsdClose(1);
}

extern bool CompareDevType(DevType left, DevType right);

TEST_F(RuntimeTest, EventTest610)
{
    MOCKER(CompareDevType)
    .stubs()
    .with(any(), eq(static_cast<s32>(DevType::DEV_TYPE_310P1)))
    .will(returnValue(true));

    rtNotify_t notify;
    HcclResult ret = hrtNotifyCreate(0, &notify);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 offset = 0;
    ret = hrtNotifyGetOffset(notify, offset);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = hrtNotifyWait(notify, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtNotifyWaitWithTimeOut(notify, stream.ptr(), 100);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

s32 fake_rtGetSocVersionV80(char *chipVer, const u32 maxLen)
{
    sal_memcpy(chipVer, sizeof("Ascend910"), "Ascend910", sizeof("Ascend910"));
    return DRV_ERROR_NONE;
}

s32 fake_rtGetSocVersionV81(char *chipVer, const u32 maxLen)
{
    sal_memcpy(chipVer, sizeof("Ascend910B1"), "Ascend910B1", sizeof("Ascend910B1"));
    return DRV_ERROR_NONE;
}

s32 fake_rtGetSocVersionV51(char *chipVer, const u32 maxLen)
{
    sal_memcpy(chipVer, sizeof("Ascend310P3"), "Ascend310P3", sizeof("Ascend310P3"));
    return DRV_ERROR_NONE;
}

TEST_F(RuntimeTest, test_NotifySize)
{
    HcclResult ret;
    u32 notifySize = 0;
    DevType deviceType;

    MOCKER(rtGetSocVersion)
    .stubs()
    .will(invoke(fake_rtGetSocVersionV80));

    deviceType = DevType::DEV_TYPE_910;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    ret = hrtGetNotifySize(notifySize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifySize, 8);
    GlobalMockObject::verify();

    MOCKER(rtGetSocVersion)
    .stubs()
    .will(invoke(fake_rtGetSocVersionV81));

    deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    ret = hrtGetNotifySize(notifySize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifySize, 4);
    GlobalMockObject::verify();

    MOCKER(rtGetSocVersion)
    .stubs()
    .will(invoke(fake_rtGetSocVersionV51));

    deviceType = DevType::DEV_TYPE_310P3;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    ret = hrtGetNotifySize(notifySize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifySize, 8);
    GlobalMockObject::verify();
}

TEST_F(HALTest, test_hrtHalSubmitEvent)
{
    MOCKER(halEschedSubmitEvent)
    .expects(once())
    .will(returnValue(DRV_ERROR_NONE));
    DlHalFunction::GetInstance().DlHalFunctionInit();
    u32 devId = 0;
    u32 eventId = HCCL_EVENT_RECV_REQUEST_MSG;
    HcclResult ret = hrtHalSubmitEvent(devId, eventId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HALTest, test_hrtHalHostRegister)
{
    void *addr = new (std::nothrow) u64[10];
    u64 size = 10;
    u32 flag;
    u32 devid;
    void *dev;
    HcclResult ret = hrtHalHostRegister(addr, size, flag, devid, dev);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64* addr1 = new (std::nothrow) u64[256 * 1024 * 1024 / sizeof(u64)];
    ret = hrtHalHostRegister(addr1, 256 * 1024 * 1024, flag, devid, dev);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete[] static_cast<u64 *>(addr);
    delete[] addr1;
}

TEST_F(HALTest, test_hrtHalHostUnregister)
{
    void *addr = new u32(0);
    u32 devid;
    HcclResult ret = hrtHalHostUnregister(addr, devid);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete addr;
}

TEST_F(HALTest, test_hrtHalHostUnregisterExNull)
{
    void *addr = new u32(0);
    u32 devid;
    DlHalFunction::GetInstance().dlHalHostUnregisterEx = nullptr;
    HcclResult ret = hrtHalHostUnregisterEx(addr, devid, HOST_MEM_MAP_DEV_PCIE_TH);
    EXPECT_EQ(ret, HCCL_E_DRV);
    delete addr;
}

TEST_F(HALTest, test_hrtHalHostUnregisterEx)
{
    void *addr = new u32(0);
    u32 devid;
    DlHalFunction::GetInstance().DlHalFunctionInit();
    HcclResult ret = hrtHalHostUnregisterEx(addr, devid, HOST_MEM_MAP_DEV_PCIE_TH);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete addr;
}

TEST_F(HALTest, test_hrtHalSensorNodeRegister)
{
    uint64_t handle = 0;
    DlHalFunction::GetInstance().DlHalFunctionInit();
    auto ret = hrtHalSensorNodeRegister(0, &handle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HALTest, test_hrtHalSensorNodeUnregister)
{
    uint64_t handle = 1;
    DlHalFunction::GetInstance().DlHalFunctionInit();
    auto ret = hrtHalSensorNodeUnregister(0, handle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HALTest, test_hrtHalSensorNodeUpdateState)
{
    uint64_t handle = 1;
    DlHalFunction::GetInstance().DlHalFunctionInit();
    auto ret = hrtHalSensorNodeUpdateState(0, handle,
        HAL_GENERAL_SOFTWARE_FAULT_NORMAL_RESOURCE_RECYCLE_FAILED, HcclGeneralEventType::HCCL_GENERAL_EVENT_TYPE_ONE_TIME);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtRaQpNonBlockConnectAsync)
{
    HcclResult ret = HCCL_SUCCESS;
    void *handle = nullptr;
    void *sockHandle = nullptr;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(RaQpConnectAsync)
    .expects(once())
    .will(returnValue(0));
    ret = HrtRaQpNonBlockConnectAsync(handle, sockHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER(RaQpConnectAsync)
    .expects(once())
    .will(returnValue(SOCK_EAGAIN));
    ret = HrtRaQpNonBlockConnectAsync(handle, sockHandle);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();

    MOCKER(RaQpConnectAsync)
    .expects(once())
    .will(returnValue(-1));
    ret = HrtRaQpNonBlockConnectAsync(handle, sockHandle);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_hrtRaSocketNonBlockBatchConnect)
{
    HcclResult ret = HCCL_SUCCESS;
    SocketConnectInfoT conn;
    u32 num = 1;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(RaSocketBatchConnect)
    .expects(once())
    .will(returnValue(0));
    ret = hrtRaSocketNonBlockBatchConnect(&conn, num);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER(RaSocketBatchConnect)
    .expects(once())
    .will(returnValue(SOCK_EAGAIN));
    ret = hrtRaSocketNonBlockBatchConnect(&conn, num);
    EXPECT_EQ(ret, HCCL_E_AGAIN);
    GlobalMockObject::verify();

    MOCKER(RaSocketBatchConnect)
    .expects(once())
    .will(returnValue(-1));
    ret = hrtRaSocketNonBlockBatchConnect(&conn, num);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_hrtRaNonBlockGetSockets)
{
    HcclResult ret = HCCL_SUCCESS;
    u32 role = 0;
    SocketInfoT conn;
    u32 num = 1;
    u32 connectedNum = 0;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(RaGetSockets)
    .expects(once())
    .will(returnValue(0));
    ret = hrtRaNonBlockGetSockets(role, &conn, num, &connectedNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER(RaGetSockets)
    .expects(once())
    .will(returnValue(SOCK_EAGAIN));
    ret = hrtRaNonBlockGetSockets(role, &conn, num, &connectedNum);
    EXPECT_EQ(ret, HCCL_E_AGAIN);
    GlobalMockObject::verify();

    MOCKER(RaGetSockets)
    .expects(once())
    .will(returnValue(-1));
    ret = hrtRaNonBlockGetSockets(role, &conn, num, &connectedNum);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_hrtRaSocketNonBlockSend)
{
    HcclResult ret = HCCL_SUCCESS;
    void *fdHandle = nullptr;
    void *data = nullptr;
    u64 size = 0;
    u64 sentSize = 0;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 result = 0;
    MOCKER(RaSocketSend)
    .expects(once())
    .will(returnValue(0));
    result = hrtRaSocketNonBlockSend(fdHandle, data, size, &sentSize);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();

    MOCKER(RaSocketSend)
    .expects(once())
    .will(returnValue(SOCK_EAGAIN));
    result = hrtRaSocketNonBlockSend(fdHandle, data, size, &sentSize);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();

    MOCKER(RaSocketSend)
    .expects(once())
    .will(returnValue(-1));
    result = hrtRaSocketNonBlockSend(fdHandle, data, size, &sentSize);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();

    size = SOCKET_SEND_MAX_SIZE + 1;
    result = hrtRaSocketNonBlockSend(fdHandle, data, size, &sentSize);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_hrtRaSocketNonBlockRecv)
{
    HcclResult ret = HCCL_SUCCESS;
    void *fdHandle = nullptr;
    void *data = nullptr;
    u64 size = 0;
    u64 recvSize = 0;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 result = 0;
    MOCKER(RaSocketRecv)
    .expects(once())
    .will(returnValue(0));
    result = hrtRaSocketNonBlockRecv(fdHandle, data, size, &recvSize);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();

    MOCKER(RaSocketRecv)
    .expects(once())
    .will(returnValue(SOCK_EAGAIN));
    result = hrtRaSocketNonBlockRecv(fdHandle, data, size, &recvSize);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();

    MOCKER(RaSocketRecv)
    .expects(once())
    .will(returnValue(-1));
    result = hrtRaSocketNonBlockRecv(fdHandle, data, size, &recvSize);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_hrtRaSocketRecv)
{
    s64 ret = 0;
    void *fdHandle = nullptr;
    void *data = nullptr;
    u64 size = 0;
    u64 recvSize = 0;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    struct RaInitConfig config = { DEFAULT_INIT_PHY_ID, DEFAULT_INIT_NIC_POS, DEFAULT_HDC_TYPE };
    ret = HrtRaInit(&config);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(RaInit)
    .stubs()
    .will(returnValue(328002));
    ret = HrtRaInit(&config);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ret = hrtRaSocketRecv(fdHandle, data, size, &recvSize);
    EXPECT_EQ(ret, 1);

    MOCKER(RaDeinit)
    .stubs()
    .will(returnValue(HCCP_EAGAIN))
    .then(returnValue(0));

    ret = HrtRaDeInit(&config);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtRaCreateCq)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* handle = (void*)0x01;
    struct CqAttr attr;
    attr.ibSendCq = (struct ibv_cq **)0x01;
    attr.ibRecvCq = (struct ibv_cq **)0x01;
    attr.qpContext = (void **)0x01;

    ret = hrtRaCreateCq(handle, &attr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtRaDestroyCq)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* handle = (void*)0x01;
    struct CqAttr* attr = (struct CqAttr*)malloc(16);
    attr->qpContext = nullptr;

    ret = hrtRaDestroyCq(handle, attr);
    EXPECT_EQ(ret, 0);
    free(attr);
}

TEST_F(RuntimeTest, test_hrtRaNormalQpCreate)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* handle = (void*)0x01;
    struct ibv_qp_init_attr initAttr;
    initAttr.qp_type = IBV_QPT_RC;
    void* qpHandle = nullptr;
    struct ibv_qp* qp = nullptr;

    MOCKER(RaNormalQpCreate)
    .expects(once())
    .will(returnValue(0));
    ret = hrtRaNormalQpCreate(handle, &initAttr, qpHandle, qp);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_HrtRaNormalQpCreate_When_OOM_Expect_ReturnIsHCCL_E_OOM)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* handle = (void*)0x01;
    struct ibv_qp_init_attr initAttr;
    initAttr.qp_type = IBV_QPT_RC;
    void* qpHandle = nullptr;
    struct ibv_qp* qp = nullptr;

    MOCKER(RaNormalQpCreate)
    .expects(once())
    .will(returnValue(328100));
    ret = hrtRaNormalQpCreate(handle, &initAttr, qpHandle, qp);
    EXPECT_EQ(ret, HCCL_E_OOM);
}

TEST_F(RuntimeTest, test_hrtRaNormalQpDestroy)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* qpHandle = (void*)0x01;

    ret = hrtRaNormalQpDestroy(qpHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtReduceAsyncV2)
{
    MOCKER(rtReduceAsyncV2)
    .expects(atMost(1))
    .will(returnValue(0));

    HcclResult ret = HCCL_SUCCESS;
    u64 srcData;
    u64 dstData;
    u64 overflow;
    void* dst = &dstData;
    uint64_t destMax = sizeof(u64);
    void* src = &srcData;
    uint64_t count = sizeof(u64);
    rtRecudeKind_t reduceKind = RT_MEMCPY_SDMA_AUTOMATIC_ADD;
    rtDataType_t dType = RT_DATA_TYPE_FP32;
    rtStream_t stream;
    void* overflowAddr = &overflow;
    ret = hrtReduceAsyncV2(dst, destMax, src, count, reduceKind, dType, stream, overflowAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_SetQpAttrQos)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrQos)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_TC", "4", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    QpHandle qpHandle = nullptr;
    ret = SetQpAttrQos(qpHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    setenv("HCCL_RDMA_TC", "132", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_RDMA_TC ");

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_hrtRaGetNotifyMrInfo)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_NOT_SUPPORT));

    void *handle;
    MrInfoT *mrInfo;
    HcclResult ret = HrtRaGetNotifyMrInfo(0, handle, mrInfo);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(RuntimeTest, test_SetQpAttrTimeOut)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_TIMEOUT", "8", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    QpHandle qpHandle = nullptr;
    ret = SetQpAttrTimeOut(qpHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 将HCCL_RDMA_TIMEOUT环境变量设为默认值，防止影响其它用例
    setenv("HCCL_RDMA_TIMEOUT", "20", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_RDMA_TIMEOUT");

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_SetQpAttrRetryCnt)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrRetryCnt)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_RETRY_CNT", "2", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    QpHandle qpHandle = nullptr;
    ret = SetQpAttrRetryCnt(qpHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 将HCCL_RDMA_RETRY_CNT环境变量设置为默认值
    setenv("HCCL_RDMA_RETRY_CNT", "7", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_RDMA_RETRY_CNT");

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_hrtRaRdmaInitWithAttr)
{
    int rdma = 0;
    struct RdevInitInfo init_info {0};
    struct rdev rdevInfo;
    RdmaHandle rdmaHandle = &rdma;
    int ret = HrtRaRdmaInitWithAttr(init_info, rdevInfo, rdmaHandle);
    EXPECT_EQ(ret, 19);
}

TEST_F(RuntimeTest, test_hrtRaRdmaGetHandle)
{
    HcclResult ret;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    int rdma = 0;
    unsigned int phyId = 0;
    RdmaHandle rdmaHandle = &rdma;
    ret = HrtRaRdmaGetHandle(phyId, rdmaHandle);

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_CteateQpWithCq)
{
    MOCKER(hrtDrvGetPlatformInfo)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaCreateCq)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaNormalQpCreate)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HrtRaQpCreate)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(SetQpAttrQos)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    struct ibv_comp_channel sendChannel;
    struct ibv_comp_channel recvChannel;

    QpInfo info;
    int ret = CreateQpWithCq(nullptr, 32, 32, &sendChannel, &recvChannel, info);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_hrtRaCreateCompChannel)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    struct RaInitConfig config= { DEFAULT_INIT_PHY_ID, DEFAULT_INIT_NIC_POS, DEFAULT_HDC_TYPE };
    ret = HrtRaInit(&config);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 rdma = 0;
    RdmaHandle rdmaHandle = &rdma;
    void **compChannel = &rdmaHandle;

    ret = hrtRaCreateCompChannel(rdmaHandle, compChannel);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HrtRaDeInit(&config);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtRaDestroyCompChannel)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    struct RaInitConfig config= { DEFAULT_INIT_PHY_ID, DEFAULT_INIT_NIC_POS, DEFAULT_HDC_TYPE };
    ret = HrtRaInit(&config);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 rdma = 0;
    RdmaHandle rdmaHandle = &rdma;
    void *compChannel = &rdma;

    ret = hrtRaDestroyCompChannel(rdmaHandle, compChannel);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HrtRaDeInit(&config);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtReduceAsyncWithCfg)
{
    HcclResult ret = HCCL_SUCCESS;

    int dstBuf = 1;
    int srcBuf = 2;
    u64 destMax = 1;
    u64 count = 1;
    rtRecudeKind_t reduceKind = RT_MEMCPY_SDMA_AUTOMATIC_ADD;
    rtDataType_t dType = RT_DATA_TYPE_FP32;
    rtStream_t stream;
    u32 qosCfg = 123;

    ret = hrtReduceAsyncWithCfg(&dstBuf, destMax, &srcBuf, count, reduceKind, dType, stream, qosCfg);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtCallbackLaunch)
{
    MOCKER(aclrtLaunchCallback)
        .stubs()
        .with(any())
        .will(returnValue(ACL_SUCCESS));
    s32 ret = HCCL_SUCCESS;

    rtStream_t stream;
    ret = rtStreamCreate(&stream, 5);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(stream);

    bool isBlock = true;
    auto callBackFunc = [](void *fnData) {
        auto data = static_cast<int*>(fnData);
        *data = 1;
    };
    void *fnData = new int(0);

    ret = hrtCallbackLaunch(callBackFunc, fnData, stream, isBlock);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    int *intPtr = static_cast<int *>(fnData);
    delete intPtr;
}

TEST_F(RuntimeTest, test_hrtStreamWaitEvent)
{
    MOCKER(aclrtCreateEvent)
        .stubs()
        .with(any())
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtStreamWaitEvent)
        .stubs()
        .with(any())
        .will(returnValue(ACL_SUCCESS));
    s32 ret = HCCL_SUCCESS;

    rtStream_t stream;
    ret = rtStreamCreate(&stream, 5);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(stream);

    aclrtEvent event;
    ret = aclrtCreateEvent(&event);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(event);

    ret = hrtStreamWaitEvent(stream, event);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtUnSubscribeReport)
{   
    MOCKER(aclrtSubscribeReport)
        .stubs()
        .with(any())
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtUnSubscribeReport)
        .stubs()
        .with(any())
        .will(returnValue(ACL_SUCCESS));
    s32 ret = HCCL_SUCCESS;

    uint64_t threadId = 0;
    rtStream_t stream;
    ret = rtStreamCreate(&stream, 5);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(stream);

    ret = hrtSubscribeReport(threadId, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtUnSubscribeReport(threadId, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtMemAsyncCopyWithCfg)
{
    HcclResult ret = HCCL_SUCCESS;

    int dstBuf = 1;
    int srcBuf = 2;
    u64 destMax = 1;
    u64 count = 1;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_DEVICE;
    rtStream_t stream;
    u32 qosCfg = 123;

    hrtStreamCreateWithFlags(&stream, HCCL_STREAM_PRIORITY_HIGH, RT_STREAM_FAST_LAUNCH | RT_STREAM_FAST_SYNC);
    ret = hrtMemAsyncCopyWithCfg(&dstBuf, destMax, &srcBuf, count, kind, stream, qosCfg);
    hrtStreamDestroy(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtRaGetCqeErrInfo_001)
{
    HcclResult ret = HCCL_SUCCESS;
    struct CqeErrInfo info;

    ret = hrtRaGetCqeErrInfo(0, &info);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_hrtRaGetQpAttr_001)
{
    HcclResult ret = HCCL_SUCCESS;
    QpHandle qpHandle;
    struct QpAttr attr = {0};

    ret = hrtRaGetQpAttr(qpHandle, &attr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_CreateQpWithSharedCq)
{
    MOCKER(hrtDrvGetPlatformInfo)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    RdmaHandle rdmaHandle;
    HcclIpAddress selfIp;
    HcclIpAddress peerIp;
    s32 sqEvent = 0;
    s32 rqEvent = 0;
    QpInfo info;

    MOCKER(CreateCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CreateNormalQp, HcclResult (*)(RdmaHandle, QpInfo&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CreateCqAndQp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = CreateQpWithSharedCq(rdmaHandle, selfIp, peerIp, sqEvent, rqEvent, info);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_CreateQpWithCq)
{
    MOCKER(hrtDrvGetPlatformInfo)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    RdmaHandle rdmaHandle;
    void * recvChannel;
    void * sendChannel;
    s32 sqEvent = 0;
    s32 rqEvent = 0;
    QpInfo info;

    MOCKER(HrtRaQpCreate)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    ret = CreateQpWithCq(rdmaHandle, sqEvent, rqEvent, sendChannel, recvChannel, info, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(CreateCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CreateNormalQp, HcclResult (*)(RdmaHandle, QpInfo&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CreateQp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = CreateQpWithCq(rdmaHandle, sqEvent, rqEvent, sendChannel, recvChannel, info);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, test_CreateCq)
{
    MOCKER(hrtRaCreateCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    RdmaHandle rdmaHandle;
    CqInfo cq;

    ret = CreateCq(rdmaHandle, cq);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_CreateNormalQp)
{
    MOCKER(hrtRaNormalQpCreate)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(SetQpAttrQos)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(SetQpAttrTimeOut)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(SetQpAttrRetryCnt)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    RdmaHandle rdmaHandle;
    QpInfo cq;

    ret = CreateNormalQp(rdmaHandle, cq);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, test_hrtRaGetDevCapInfo)
{
    HcclResult ret = HCCL_SUCCESS;
    RdmaHandle rdmaHandle;
    SrqInfo srqInfo;

    ret = hrtRaCreateSrq(rdmaHandle, srqInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

const unsigned int TOPOLOGY_CONVERT[4][4] = {
    TOPOLOGY_PIX, TOPOLOGY_SIO, TOPOLOGY_HCCS_SW, TOPOLOGY_HCCS_SW,
    TOPOLOGY_SIO, TOPOLOGY_PIX, TOPOLOGY_HCCS_SW, TOPOLOGY_HCCS_SW,
    TOPOLOGY_HCCS_SW, TOPOLOGY_HCCS_SW, TOPOLOGY_PIX, TOPOLOGY_SIO,
    TOPOLOGY_HCCS_SW, TOPOLOGY_HCCS_SW, TOPOLOGY_SIO, TOPOLOGY_PIX};

HcclResult stub_hrtGetPairDevicesInfo(u32 phyDevId, u32 otherPhyDevId, s32 infoType, s64 *pValue)
{
    if (phyDevId > 3 || otherPhyDevId > 3) {
        *pValue = 0;
    } else {
        *pValue = TOPOLOGY_CONVERT[phyDevId][otherPhyDevId];
    }

    return HCCL_SUCCESS;
}

TEST_F(RuntimeTest, test_hrtGetPairDevicesInfo)
{
    MOCKER(hrtGetPairDevicesInfo)
    .stubs()
    .will(invoke(stub_hrtGetPairDevicesInfo));
    std::unordered_map<u32, u32> pairLinkCounter;
    pairLinkCounter[static_cast<u32>(LinkTypeInServer::SIO_TYPE)] = 0;
    pairLinkCounter[static_cast<u32>(LinkTypeInServer::HCCS_SW_TYPE)] = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i == j) {
                continue;
            }
            LinkTypeInServer linkType;
            hrtGetPairDeviceLinkType(i, j, linkType);
            pairLinkCounter[static_cast<u32>(linkType)]++;
        }
    }

    EXPECT_EQ(pairLinkCounter[static_cast<u32>(LinkTypeInServer::SIO_TYPE)], 4);
    EXPECT_EQ(pairLinkCounter[static_cast<u32>(LinkTypeInServer::HCCS_SW_TYPE)], 8);
    GlobalMockObject::verify();
}

rtError_t stub_rtGetPairPhyDevicesInfo(uint32_t phyDevId, uint32_t otherPhyDevId, int32_t infoType, int64_t *pValue)
{
    if (phyDevId > 3 || otherPhyDevId > 3) {
        *pValue = 0;
    } else {
        *pValue = TOPOLOGY_CONVERT[phyDevId][otherPhyDevId];
    }
 
    return ACL_RT_SUCCESS;
}

TEST_F(RuntimeTest, test_hrtGetPairPhyDevicesInfo)
{
    auto funPtr = reinterpret_cast<int (*)(unsigned int, unsigned int, int, long int*)>(&stub_rtGetPairPhyDevicesInfo);
 
    s64 linkTypeRaw = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i == j) {
                continue;
            }
            hrtGetPairPhyDevicesInfo(i, j, 0, &linkTypeRaw, funPtr);
            if (i > 3 || j > 3) {
                EXPECT_EQ(linkTypeRaw, 0);
            } else {
                EXPECT_EQ(linkTypeRaw, TOPOLOGY_CONVERT[i][j]);
            }
        }
    }
    GlobalMockObject::verify();
}

/* 设备内存拷贝测试用例 */
TEST_F(RuntimeTest, ut_rt_mem_copy_hugedata_test)
{
    void* dev_src_ptr = NULL;
    void* dev_dst_ptr = NULL;
    char* src_ptr = NULL;
    char* dst_ptr = NULL;
    char     src_value = 0xAA;
    s32     mem_size = 0x19000000;

    HcclRtStream  stream;
    s32 ret = HCCL_SUCCESS;
    s32    device_id = 0;

    MOCKER(HcomCheckrtMemcpyAddrAsync)
    .stubs()
    .with(any())
    .will(returnValue(false));

    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    ret = rtStreamCreate(&stream, 5);
    Stream hcclStream(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    if (RT_ERROR_NONE != ret)
    { goto destroy; }

    /* 申请源设备内存空间 */
    ret = hrtMalloc(&dev_src_ptr, mem_size);
    EXPECT_NE((long)dev_src_ptr, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (NULL == dev_src_ptr)
    { goto destroy; }

    /* 申请目的设备内存空间 */
    ret = hrtMalloc(&dev_dst_ptr, mem_size);
    EXPECT_NE((long)dev_dst_ptr, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (NULL == dev_dst_ptr)
    {
        goto destroy;
    }

    /* 申请源Host内存空间 */
    src_ptr = (char*)sal_malloc(mem_size);
    EXPECT_NE((long)src_ptr, NULL);
    if (NULL == src_ptr)
    {
        goto destroy;
    }

    /* 申请目的Host内存空间 */
    dst_ptr = (char*)sal_malloc(mem_size);
    EXPECT_NE((long)dst_ptr, NULL);
    if (NULL == dst_ptr)
    {
        goto destroy;
    }

    /* 源内存初始化 */
    ret = sal_memset(src_ptr, mem_size, src_value, mem_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (HCCL_SUCCESS != ret)
    {
        goto destroy;
    }

    /* 校验源内存初始化结果 */
    EXPECT_EQ(*src_ptr, src_value);
    EXPECT_EQ(*(src_ptr + mem_size - 1), src_value);

    /* 清空Host目的内存 */
    ret = sal_memset(dst_ptr, mem_size, 0, mem_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (HCCL_SUCCESS != ret)
    {
        goto destroy;
    }


    ret = dispatcher->MemcpyAsync(dst_ptr, mem_size,src_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_HOST, hcclStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    /* 校验内存拷贝结果 */
    EXPECT_EQ(*dst_ptr, src_value);
    EXPECT_EQ(*(dst_ptr + mem_size - 1), src_value);


    /* 清空Host目的内存 */
    ret = sal_memset(dst_ptr, mem_size, 0, mem_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (HCCL_SUCCESS != ret)
    {
        goto destroy;
    }
    ret = dispatcher->MemcpyAsync(dev_src_ptr,mem_size, src_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE, hcclStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->MemcpyAsync(dev_dst_ptr,mem_size, dev_src_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_DEVICE, hcclStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = dispatcher->MemcpyAsync(dst_ptr, mem_size,dev_dst_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST, hcclStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    /* 校验内存拷贝结果 */
    EXPECT_EQ(*dst_ptr, src_value);
    EXPECT_EQ(*(dst_ptr + mem_size - 1), src_value);

    /* 异常参数覆盖 */
    ret = dispatcher->MemcpyAsync(dst_ptr, mem_size,src_ptr, mem_size,  HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_RESERVED, hcclStream);
    EXPECT_NE(ret, HCCL_SUCCESS);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

destroy:
    /* 释放内存空间 */
    HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
    HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
    RELEASE_DEVICE_PTR(dev_src_ptr);
    RELEASE_DEVICE_PTR(dev_dst_ptr);
    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);
}

TEST_F(RuntimeTest, ut_rt_inline_reduce_hugedata_test)
{
    s32 ret = HCCL_SUCCESS;
    HcclRtStream rtStream;
    Stream stream;
    s32 ret_for_result = HCCL_SUCCESS;

    char* src1_char = NULL;
    char* dst_char = NULL;
    float* src1_float = NULL;
    float* dst_float = NULL;
    char tempdst_char;
    float tempdst_float;
    u32 count = 0x6400000;

    s32 device_id = 0;
    ret =  hrtSetDevice(device_id);

    // 申请内存空间(char)
    ret = hrtMalloc((void**)&src1_char, count);
    EXPECT_NE((long)src1_char, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (NULL == src1_char)
    {
        goto destroy;
    }

    memset(src1_char, 1, count);

    ret = hrtMalloc((void**)&dst_char, count);
    EXPECT_NE((long)dst_char, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (NULL == dst_char)
    {
        goto destroy;
    }

    memset(dst_char, 0, count);

    // 申请内存空间(float)
    ret = hrtMalloc((void**)&src1_float, count * 4);
    EXPECT_NE((long)src1_float, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (NULL == src1_float)
    {
        goto destroy;
    }

    ret = hrtMalloc((void**)&dst_float, count * 4);
    EXPECT_NE((long)dst_float, NULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (NULL == dst_float)
    {
        goto destroy;
    }

    for (s32 i = 0; i < count; i++)
    {
        src1_float[i] = 1.0;
        dst_float[i] = 2.0;
    }

    // 申请stream
    ret = rtStreamCreate(&rtStream, 5);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    stream = Stream(rtStream);
    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    // reduce_float op_sum
    ret = dispatcher->InlineReduceAsync((void*)src1_float, count, HcclDataType::HCCL_DATA_TYPE_FP32, HcclReduceOp::HCCL_REDUCE_SUM, stream, (void*)dst_float);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    ret = rtStreamSynchronize(rtStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (ret != HCCL_SUCCESS)
    { goto destroy; }

    for (s32 i = 0; i < count; i++)
    {
        tempdst_float = dst_float[i];

        if (tempdst_float != 3.0)
        {
            ret_for_result = HCCL_E_INTERNAL;
            break;
        }
    }

    EXPECT_EQ(ret_for_result, HCCL_SUCCESS);

    if (ret_for_result != HCCL_SUCCESS)
    { goto destroy; }
    // 释放资源
destroy:
    if (src1_char != NULL)
    {
        ret = hrtFree((void*)src1_char);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    if (dst_char != NULL)
    {
        ret = hrtFree((void*)dst_char);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    if (src1_float != NULL)
    {
        ret = hrtFree((void*)src1_float);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    if (dst_float != NULL)
    {
        ret = hrtFree((void*)dst_float);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    if (rtStream != NULL)
    {
        // 销毁资源
        ret = rtStreamDestroy(rtStream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}

TEST_F(RuntimeTest, ut_rt_ra_epoll)
{
    HcclResult ret;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<SocketEventInfo> eventInfos(1);
    struct socket_peer_info info;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    info.fd = sockfd;
    unsigned int events_num = 0;
    int epollFd;
    FdHandle fd = (void*)&info;

    ret = hrtRaCreateEventHandle(epollFd);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtRaCtlEventHandle(epollFd, fd, EPOLL_CTL_ADD, HcclEpollEvent::HCCL_EPOLLIN);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtRaWaitEventHandle(epollFd, eventInfos, 1, 1, events_num);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtRaDestroyEventHandle(epollFd);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    close(sockfd);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_rt_hrt_notify_reset_test)
{
    int nty = 1;
    HcclRtNotify notify = &nty;
    auto ret = hrtNotifyReset(notify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_rt_hrt_task_abort_callback_test)
{
    int32_t ProcessTaskAbortHandleCallback(uint32_t devId, rtDeviceTaskAbortStage stage,
                                           uint32_t timeout, void *args);
    void *ptr = nullptr;
    auto ret = hrtTaskAbortHandleCallback(ProcessTaskAbortHandleCallback, nullptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_rt_hrt_Resource_Clean_test)
{
    int32_t devId = 1;
    rtIdType_t type = RT_NOTIFY_ID;
    auto ret = hrtResourceClean(devId, type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_hrtRaRdevGetPortStatus)
{
    DlRaFunction::GetInstance().DlRaFunctionInit();
    int a = 0;
    RdmaHandle handle = &a;
    enum PortStatus *status;
    hrtRaRdevGetPortStatus(handle, status);
}

TEST_F(RuntimeTest, ut_hrtRaGetCqeErrInfoList)
{
    DlRaFunction::GetInstance().DlRaFunctionInit();
      int a = 0;
    RdmaHandle handle = &a;
    struct CqeErrInfo err_info_list[1] = {};
    u32 num = 0;
    hrtRaGetCqeErrInfoList(handle, err_info_list, &num);
}

TEST_F(RuntimeTest, ut_hrtGetPairDevicePhyId_0)
{
    DevType devType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(devType))
    .will(returnValue(HCCL_SUCCESS));

    LinkTypeInServer linkType = LinkTypeInServer::SIO_TYPE;
    MOCKER(hrtGetPairDeviceLinkType)
    .stubs()
    .with(any(), any(), outBound(linkType))
    .will(returnValue(HCCL_SUCCESS));

    u32 localPhyId = 0;
    u32 pairPhyId = 0;
    auto ret = hrtGetPairDevicePhyId(localPhyId, pairPhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(pairPhyId, 1);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_hrtGetPairDevicePhyId_16)
{
    DevType devType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(devType))
    .will(returnValue(HCCL_SUCCESS));

    LinkTypeInServer linkTypeHCCS = LinkTypeInServer::HCCS_TYPE;
    MOCKER(hrtGetPairDeviceLinkType)
    .stubs()
    .with(any(), any(), outBound(linkTypeHCCS))
    .will(returnValue(HCCL_SUCCESS));

    u32 localPhyId = 16;
    u32 pairPhyId = 0;
    auto ret = hrtGetPairDevicePhyId(localPhyId, pairPhyId);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_hrtRdmaInitWithBackupAttr_notSupport)
{
    struct RdevInitInfo redvInitInfo;
    redvInitInfo.mode = 1;
    redvInitInfo.notifyType = 0;
    redvInitInfo.enabled910aLite = false;
    redvInitInfo.disabledLiteThread = false;
    redvInitInfo.enabled2mbLite = false;

    struct rdev rdevInfo;
    rdevInfo.phyId = 0;
    struct rdev backupRdevInfo;
    backupRdevInfo.phyId = 1;
    RdmaHandle rdmaHandle;

    MOCKER(hrtRaGetInterfaceVersion).expects(atMost(1)).will(returnValue(HCCL_E_NOT_SUPPORT));
    auto ret = HrtRdmaInitWithBackupAttr(redvInitInfo, rdevInfo, backupRdevInfo, rdmaHandle);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    GlobalMockObject::verify();
}

rtError_t rtGetDeviceInfo_stub(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *value)
{
    static std::array<int64_t,7> vals = {0x0,0x1c,0x1d,0x18,0x19,0x14,0x15};
    static int flag = 0;
    if (!flag) {
        ++flag;
        return 0x07110001;
    }
    *value = vals[flag++-1];
    return RT_ERROR_NONE;
}

TEST_F(RuntimeTest, ut_hrtGetHccsPortNum)
{
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    s32 portNum = 0;
    EXPECT_EQ(hrtGetHccsPortNum(0, portNum), HCCL_E_NOT_SUPPORT);
    GlobalMockObject::verify();

    deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(deviceType))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(rtGetDeviceInfo)
            .stubs()
            .will(invoke(rtGetDeviceInfo_stub));
    EXPECT_EQ(hrtGetHccsPortNum(0, portNum), HCCL_E_RUNTIME);
    EXPECT_EQ(hrtGetHccsPortNum(0, portNum), HCCL_SUCCESS);
    EXPECT_EQ(portNum, -1);
    EXPECT_EQ(hrtGetHccsPortNum(0, portNum), HCCL_SUCCESS);
    EXPECT_EQ(portNum, 6);
    EXPECT_EQ(hrtGetHccsPortNum(0, portNum), HCCL_SUCCESS);
    EXPECT_EQ(portNum, 6);
    EXPECT_EQ(hrtGetHccsPortNum(0, portNum), HCCL_SUCCESS);
    EXPECT_EQ(portNum, 7);
    EXPECT_EQ(hrtGetHccsPortNum(0, portNum), HCCL_SUCCESS);
    EXPECT_EQ(portNum, 7);
    EXPECT_EQ(hrtGetHccsPortNum(0, portNum), HCCL_SUCCESS);
    EXPECT_EQ(portNum, 7);
    EXPECT_EQ(hrtGetHccsPortNum(0, portNum), HCCL_SUCCESS);
    EXPECT_EQ(portNum, 7);
}

TEST_F(RuntimeTest, ut_ra_socket_listen_stop_error)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(RaSocketListenStop)
    .stubs()
    .will(returnValue(SOCK_EADDRINUSE));

    SocketListenInfoT sockListen;
    ret = hrtRaSocketListenStop(&sockListen, 1);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_hrtMemAsyncCopyWithoutCheckKind)
{
    HcclResult ret = HCCL_SUCCESS;
 
    int dstBuf = 1;
    int srcBuf = 2;
    u64 destMax = 1;
    u64 count = 1;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE;
    rtStream_t stream;
 
    MOCKER(rtMemcpyAsyncWithoutCheckKind)
    .stubs()
    .will(returnValue(RT_ERROR_NONE));
 
    hrtStreamCreateWithFlags(&stream, HCCL_STREAM_PRIORITY_HIGH, RT_STREAM_FAST_LAUNCH | RT_STREAM_FAST_SYNC);
    ret = hrtMemAsyncCopyWithoutCheckKind(&dstBuf, destMax, &srcBuf, count, kind, stream);
    hrtStreamDestroy(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

HcclResult stub_hrtRaGetInterfaceVersion1(unsigned int phyId, unsigned int interfaceOpcode,
                                         unsigned int* interfaceVersion)
{
    *interfaceVersion = 1;
    return HCCL_SUCCESS;
}

TEST_F(RuntimeTest, test_GetIsSupSockBatchCloseImmed)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(invoke(stub_hrtRaGetInterfaceVersion1));

    bool isSupportBatchClose = false;
    HcclResult ret = GetIsSupSockBatchCloseImmed(0, isSupportBatchClose);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest,  test_hrtGetDeviceRefresh) 
{
    s32 deviceLogicId = 0;
    HcclResult ret;
    MOCKER(aclrtGetDevice).stubs().with(any()).will(returnValue(1)); 
    ret = hrtGetDeviceRefresh(&deviceLogicId);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
    GlobalMockObject::verify();
}
TEST_F(RuntimeTest,  test_ConstructQp_rcq_error) 
{
    s32 qpMode = 0;
    struct QpExtAttrs attrs;
    bool isWorkFlowLib = true;
    QueueDepthAttr qpDepth;
    qpDepth.recvCqDepth = 127;
    qpDepth.rqDepth = INVALID_UINT;
    qpDepth.sendCqDepth = INVALID_UINT;
    qpDepth.sqDepth = INVALID_UINT;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.recvCqDepth = 8;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.recvCqDepth = 32769;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.recvCqDepth = 32768;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_SUCCESS);

    qpDepth.recvCqDepth = 128;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest,  test_ConstructQp_rq_error) 
{
    s32 qpMode = 0;
    struct QpExtAttrs attrs;
    bool isWorkFlowLib = true;
    QueueDepthAttr qpDepth;
    qpDepth.recvCqDepth = INVALID_UINT;
    qpDepth.rqDepth = 127;
    qpDepth.sendCqDepth = INVALID_UINT;
    qpDepth.sqDepth = INVALID_UINT;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.rqDepth = 8;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.rqDepth = 32769;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.rqDepth = 32768;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_SUCCESS);

    qpDepth.rqDepth = 128;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest,  test_ConstructQp_scq_error) 
{
    s32 qpMode = 0;
    struct QpExtAttrs attrs;
    bool isWorkFlowLib = true;
    QueueDepthAttr qpDepth;
    qpDepth.recvCqDepth = INVALID_UINT;
    qpDepth.rqDepth = INVALID_UINT;
    qpDepth.sendCqDepth = 127;
    qpDepth.sqDepth = INVALID_UINT;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.sendCqDepth = 8;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.sendCqDepth = 32769;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.sendCqDepth = 32768;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_SUCCESS);

    qpDepth.sendCqDepth = 128;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest,  test_ConstructQp_sq_error) 
{
    s32 qpMode = 0;
    struct QpExtAttrs attrs;
    bool isWorkFlowLib = true;
    QueueDepthAttr qpDepth;
    qpDepth.recvCqDepth = INVALID_UINT;
    qpDepth.rqDepth = INVALID_UINT;
    qpDepth.sendCqDepth = INVALID_UINT;
    qpDepth.sqDepth = 127;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.sqDepth = 8;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.sqDepth = 32769;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_E_PARA);

    qpDepth.sqDepth = 32768;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_SUCCESS);

    qpDepth.sqDepth = 128;
    EXPECT_EQ(ConstructQpAttrs(qpMode, attrs, qpDepth, isWorkFlowLib), HCCL_SUCCESS);
    GlobalMockObject::verify();
}

HcclResult stub_hrtRaGetTlsEnable(unsigned int phyId, unsigned int interfaceOpcode,
                                         unsigned int* interfaceVersion)
{
    *interfaceVersion = 2;
    return HCCL_SUCCESS;
}

TEST_F(RuntimeTest,  test_HrtRaGetTlsEnable)
{
    bool tls_enable = false;
    struct RaInfo info;
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(invoke(stub_hrtRaGetTlsEnable));
    HcclResult ret = HrtRaGetTlsEnable(&info, &tls_enable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

HcclResult stub_hrtRaGetInterfaceVersion0(unsigned int phyId, unsigned int interfaceOpcode,
                                         unsigned int* interfaceVersion)
{
    *interfaceVersion = 0;
    return HCCL_SUCCESS;
}

TEST_F(RuntimeTest, ut_IsSupportRaSocketAbort_Support)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    bool isSupportRaSocketAbort;

    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(invoke(stub_hrtRaGetInterfaceVersion1));

    ret = IsSupportRaSocketAbort(isSupportRaSocketAbort);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(isSupportRaSocketAbort, true);

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_IsSupportRaSocketAbort_NoSupport_Version0)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    bool isSupportRaSocketAbort;

    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(invoke(stub_hrtRaGetInterfaceVersion0));

    ret = IsSupportRaSocketAbort(isSupportRaSocketAbort);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(isSupportRaSocketAbort, false);

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_IsSupportRaSocketAbort_NoSupport)
{
    HcclResult ret;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    bool isSupportRaSocketAbort;

    MOCKER(hrtRaGetInterfaceVersion)
    .stubs()
    .will(returnValue(HCCL_E_NOT_SUPPORT));

    ret = IsSupportRaSocketAbort(isSupportRaSocketAbort);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(isSupportRaSocketAbort, false);

    GlobalMockObject::verify();
}

TEST_F(RuntimeTest,  test_HcclNetDevGetTlsStatus)
{
    HcclNetDevCtx netDevCtx;
    HcclResult ret = HcclNetOpenDev(&netDevCtx, NicType::DEVICE_NIC_TYPE, 0, 0, HcclIpAddress("6.6.6.6"));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    TlsStatus status;
    ret = HcclNetDevGetTlsStatus(netDevCtx, &status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetCloseDev(netDevCtx);
}

TEST_F(RuntimeTest,  HcclNetCloseDevNullptr)
{
    HcclNetDevCtx netDevCtx = nullptr;
    HcclNetCloseDev(netDevCtx);
}

TEST_F(RuntimeTest, ut_hrtStreamCreate_and_destroy_test)
{
    s32 ret = HCCL_SUCCESS;
    HcclRtStream stream = NULL;

    s32 device_id = 0;
    ret =  hrtSetDevice(device_id);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);

    // 申请stream
    ret = hrtStreamCreate(&stream, 5);
    UT_EXPECT_EQ_OR_RETURN(ret, HCCL_SUCCESS);
    UT_CHECK_NULL_PTR(stream);

    // 销毁资源
    ret = hrtStreamDestroy(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_HrtRaQpCreate)
{
    MOCKER(RaQpCreate).stubs().will(returnValue(0));
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RdmaHandle rdmaHandle;
    int flag = 0;
    int qpMode = 0;
    QpHandle qpHandle;
    ret = HrtRaQpCreate(rdmaHandle, flag, qpMode, qpHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_HrtRaQpCreate_When_OOM_Expect_ReturnIsHCCL_E_OOM)
{
    MOCKER(RaQpCreate)
    .stubs()
    .will(returnValue(328100));
    HcclResult ret = HCCL_SUCCESS;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RdmaHandle rdmaHandle;
    int flag = 0;
    int qpMode = 0;
    QpHandle qpHandle;
    ret = HrtRaQpCreate(rdmaHandle, flag, qpMode, qpHandle);
    EXPECT_EQ(ret, HCCL_E_OOM);
    GlobalMockObject::verify();
}

TEST_F(RuntimeTest, ut_hrtRaQpCreateWithAttrs)
{
    MOCKER(RaQpCreateWithAttrs).stubs().will(returnValue(0));
    HcclResult ret = HCCL_SUCCESS;
    char tmp1[50];
    RdmaHandle rdmaHandle = &tmp1;
    char tmp2[50];
    QpHandle qpHandle = &tmp2;
    QpExtAttrs attrs;
    attrs.qpAttr.qp_type = IBV_QPT_RC;
    ret = hrtRaQpCreateWithAttrs(rdmaHandle, &attrs, qpHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_HrtRaQpCreateWithAttrs_When_OOM_Expect_ReturnIsHCCL_E_OOM)
{
    MOCKER(RaQpCreateWithAttrs)
    .stubs()
    .will(returnValue(328100));
    HcclResult ret = HCCL_SUCCESS;
    char tmp1[50];
    RdmaHandle rdmaHandle = &tmp1;
    char tmp2[50];
    QpHandle qpHandle = &tmp2;
    QpExtAttrs attrs;
    attrs.qpAttr.qp_type = IBV_QPT_RC;
    ret = hrtRaQpCreateWithAttrs(rdmaHandle, &attrs, qpHandle);
    EXPECT_EQ(ret, HCCL_E_OOM);
}

TEST_F(RuntimeTest, ut_hrtRaAiQpCreate)
{
    MOCKER(RaAiQpCreate).stubs().will(returnValue(0));
    u32 interfaceVersion = 2;
    MOCKER(hrtRaGetInterfaceVersion).expects(atMost(1)).with(any(), any(), outBoundP(&interfaceVersion)).will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    u32 phy_id = 0;
    char tmp1[50];
    RdmaHandle rdmaHandle = &tmp1;
    QpExtAttrs attrs;
    attrs.qpAttr.qp_type = IBV_QPT_RC;
    struct AiQpInfo info;
    char tmp2[50];
    QpHandle qpHandle = &tmp2;
    ret = hrtRaAiQpCreate(phy_id, rdmaHandle, &attrs, &info, qpHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RuntimeTest, ut_HrtRaAiQpCreate_When_OOM_Expect_ReturnIsHCCL_E_OOM)
{
    MOCKER(RaAiQpCreate)
    .stubs().
    will(returnValue(328100));
    u32 interfaceVersion = 2;
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .with(any(), any(), outBoundP(&interfaceVersion))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    u32 phy_id = 0;
    char tmp1[50];
    RdmaHandle rdmaHandle = &tmp1;
    QpExtAttrs attrs;
    attrs.qpAttr.qp_type = IBV_QPT_RC;
    struct AiQpInfo info;
    char tmp2[50];
    QpHandle qpHandle = &tmp2;
    ret = hrtRaAiQpCreate(phy_id, rdmaHandle, &attrs, &info, qpHandle);
    EXPECT_EQ(ret, HCCL_E_OOM);
}

TEST_F(RuntimeTest, ut_hrtCtxGetCurrent_null_ctx)
{
    MOCKER(aclrtGetCurrentContext).stubs().will(returnValue(ACL_ERROR_RT_CONTEXT_NULL));

    HcclResult ret = HCCL_SUCCESS;
    aclrtContext ctx = nullptr;
    ret = hrtCtxGetCurrent(&ctx);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(ctx, nullptr);
}
