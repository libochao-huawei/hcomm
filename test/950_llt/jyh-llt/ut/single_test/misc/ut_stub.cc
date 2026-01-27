#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <slog.h>
#include <log.h>
#include <string>

#include <driver/ascend_hal.h>
#include <runtime/rt.h>
#include <cce/hccl_api.h>

#include <sal.h>
#include <hccl/base.h>
#include <hccl/hccl_types.h>
#include <securec.h>

#include "llt_hccl_stub_sal_pub.h"
#include "runtime/rt_error_codes.h"
#include "acl/acl_rt.h"

using namespace std;

class RuntimeTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "RuntimeTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "RuntimeTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
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
/**
* @brief test reduce :8gpu,executes on gpu0
*/
TEST_F(RuntimeTest, rtGetDeviceCount)
{

    rtError_t ret = RT_ERROR_NONE;
    int32_t device_count = 0;

    ret =  rtGetDeviceCount(&device_count);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    EXPECT_EQ(device_count, 8);
}

/**
* @brief test aclrtSetDevice/aclrtGetDevice :set device 4, check get device
*/
TEST_F(RuntimeTest, aclrtSetDevice)
{

    rtError_t ret = RT_ERROR_NONE;
    int32_t device = 4;
    int32_t check_dev = 0;

    ret =  aclrtSetDevice(device);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    /*get device，检查结果是否为device*/
    ret = aclrtGetDevice(&check_dev);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    EXPECT_EQ(check_dev, device);
}

/**
* @brief test rtMemcpyAsync :拷贝字符串，检查是否一致
*/
TEST_F(RuntimeTest, rtMemcpyAsync)
{
    s32 ret;
    rtError_t rt_ret = RT_ERROR_NONE;

    s32 memcpy_buffer_size = 100;
    s8* host_src = (s8*)sal_malloc(memcpy_buffer_size);
    s8* host_dst = (s8*)sal_malloc(memcpy_buffer_size);
    void* dev_src = NULL;
    void* dev_dst = NULL;

    rtStream_t stream = NULL;

    // 准备Host数据
    if (NULL == host_src)
    {
        HCCL_ERROR("sal_malloc_stub failed, size[%d]", memcpy_buffer_size);
        goto UT_EXIT;
    }
    if (NULL == host_dst)
    {
        HCCL_ERROR("sal_malloc_stub failed, size[%d]", memcpy_buffer_size);
        goto UT_EXIT;
    }
    ret = sal_memset(host_src, memcpy_buffer_size, 0x55, memcpy_buffer_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (HCCL_SUCCESS != ret)
    {
        HCCL_ERROR("sal_memset_stub failed, ret[%d]", ret);
        goto UT_EXIT;
    }
    ret = sal_memset(host_dst, memcpy_buffer_size, 0x00, memcpy_buffer_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (HCCL_SUCCESS != ret)
    {
        HCCL_ERROR("sal_memset_stub failed, ret[%d]", ret);
        goto UT_EXIT;
    }

    // 准备Device数据
    rt_ret = rtMalloc(&dev_src, memcpy_buffer_size, RT_MEMORY_HBM, HCCL);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    if(RT_ERROR_NONE != ret)
    {
        HCCL_ERROR("rtMalloc failed, size[%d]", memcpy_buffer_size);
        goto UT_EXIT;
    }

    rt_ret = rtMalloc(&dev_dst, memcpy_buffer_size, RT_MEMORY_HBM, HCCL);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    if(RT_ERROR_NONE != ret)
    {
        HCCL_ERROR("rtMalloc failed, size[%d]", memcpy_buffer_size);
        goto UT_EXIT;
    }

    rt_ret = rtStreamCreate(&stream, 5);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    if(RT_ERROR_NONE != rt_ret)
    {
        HCCL_ERROR("rtStreamCreate failed");
        goto UT_EXIT;
    }


    // 入参检查覆盖测试
    rt_ret = rtMemcpyAsync(dev_dst, memcpy_buffer_size, dev_src, memcpy_buffer_size, RT_MEMCPY_RESERVED, stream);
    EXPECT_EQ(rt_ret, ACL_ERROR_RT_PARAM_INVALID);


    rt_ret = rtMemcpyAsync(dev_dst, memcpy_buffer_size, NULL, memcpy_buffer_size, RT_MEMCPY_DEVICE_TO_DEVICE, stream);
    EXPECT_EQ(rt_ret, ACL_ERROR_RT_PARAM_INVALID);


    rt_ret = rtMemcpyAsync(NULL, memcpy_buffer_size, dev_src, memcpy_buffer_size, RT_MEMCPY_DEVICE_TO_DEVICE, stream);
    EXPECT_EQ(rt_ret, ACL_ERROR_RT_PARAM_INVALID);

    // 功能测试
    rt_ret = rtMemcpyAsync(dev_src, memcpy_buffer_size, host_src, memcpy_buffer_size, RT_MEMCPY_HOST_TO_DEVICE, stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    if(RT_ERROR_NONE != rt_ret)
    {
        HCCL_ERROR("HOST_TO_DEVICE MemcpyAsync failed[%d]", rt_ret);
        goto UT_EXIT;
    }
    rt_ret = rtMemcpyAsync(dev_dst, memcpy_buffer_size, dev_src, memcpy_buffer_size, RT_MEMCPY_DEVICE_TO_DEVICE, stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    if(RT_ERROR_NONE != rt_ret)
    {
        HCCL_ERROR("DEVICE_TO_DEVICE MemcpyAsync failed[%d]", rt_ret);
        goto UT_EXIT;
    }

    rt_ret = rtMemcpyAsync(host_dst, memcpy_buffer_size, dev_dst, memcpy_buffer_size, RT_MEMCPY_DEVICE_TO_HOST, stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    if(RT_ERROR_NONE != rt_ret)
    {
        HCCL_ERROR("DEVICE_TO_HOST MemcpyAsync failed[%d]", rt_ret);
        goto UT_EXIT;
    }

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    if(RT_ERROR_NONE != rt_ret)
    {
        HCCL_ERROR("StreamSynchronize failed[%d]", rt_ret);
        goto UT_EXIT;
    }

    // 结果校验
    for (s32 i = 0; i < memcpy_buffer_size; i++)
    {
        EXPECT_EQ(host_src[i], host_dst[i]);
        if (host_src[i] != host_dst[i])
        {
            HCCL_ERROR("StreamSynchronize failed[%d]", rt_ret);
            goto UT_EXIT;
        }
    }


    // 释放资源及返回
UT_EXIT:
    HCCL_RELEASE_PTR_AND_SET_NULL(host_src);
    HCCL_RELEASE_PTR_AND_SET_NULL(host_dst);
    if (dev_src)
        aclrtFree(dev_src);
    if (dev_dst)
        aclrtFree(dev_dst);
    if (stream)
        rtStreamDestroy(stream);
}


/**
* @brief test aclrtCreateEvent :check create event
*/
TEST_F(RuntimeTest, aclrtCreateEvent)
{

    aclError ret = ACL_SUCCESS;
    aclrtEvent event;

    ret = aclrtCreateEvent(&event);
    EXPECT_EQ(ret, ACL_SUCCESS);

    ret = aclrtDestroyEvent(event);
    EXPECT_EQ(ret, RT_ERROR_NONE);

}

/**
* @brief test aclrtDestroyEvent :check event destroy
*/
TEST_F(RuntimeTest, aclrtDestroyEvent)
{

    aclError ret = ACL_SUCCESS;
    aclrtEvent event;

    ret = aclrtCreateEvent(&event);
    EXPECT_EQ(ret, ACL_SUCCESS);

    /*检查是否能判断event destroy 空指针*/
    ret = aclrtDestroyEvent(NULL);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = aclrtDestroyEvent(event);
    EXPECT_EQ(ret, RT_ERROR_NONE);
}


/**
* @brief test aclrtRecordEvent/aclrtQueryEventStatus :check event destroy
*/
TEST_F(RuntimeTest, aclrtRecordEvent)
{
    
    rtError_t ret = ACL_SUCCESS;
    aclrtEvent event;
    rtStream_t stream;
    ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = aclrtCreateEvent(&event);
    EXPECT_EQ(ret, ACL_SUCCESS);

    ret = aclrtRecordEvent(event, stream);
    EXPECT_EQ(ret, ACL_SUCCESS);

    aclrtEventRecordedStatus status = ACL_EVENT_RECORDED_STATUS_NOT_READY;
    ret = aclrtQueryEventStatus(event, &status);
    EXPECT_EQ(ret, ACL_SUCCESS);

    ret = aclrtDestroyEvent(event);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);
}


/**
* @brief test rtMalloc/aclrtFree:
*/
TEST_F(RuntimeTest, rtMalloc)
{
    rtError_t ret = RT_ERROR_NONE;
    void** devPtr = NULL;
    uint64_t size = 100;
    rtMemType_t type = RT_MEMORY_HBM;

    /*检查指针非空*/
    ret = rtMalloc(NULL, size, type, HCCL);
    EXPECT_EQ(ret, ACL_ERROR_RT_PARAM_INVALID);

    /*检查type*/
    devPtr = (void**)malloc(sizeof(void*));
    ret = rtMalloc(devPtr, size, RT_MEMORY_RESERVED, HCCL);
    EXPECT_EQ(ret, ACL_ERROR_RT_PARAM_INVALID);

    /*检查申请和释放是否成功*/
    ret = rtMalloc(devPtr, size, RT_MEMORY_HBM, HCCL);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    ret = aclrtFree(*devPtr);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    /*检查是否能释放空指针*/
    ret = aclrtFree(NULL);
    EXPECT_EQ(ret, ACL_ERROR_RT_PARAM_INVALID);

    free(devPtr);
}

namespace cce
{

    /**
    * @brief test ccVectorReduce
    */
    TEST_F(RuntimeTest, ccVectorReduce_int)
    {
        ccStatus_t ret = CC_STATUS_SUCCESS;
        char src1[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        char src2[10] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
        char dst[10] ;
        uint32_t count = 10;
        ccDataType_t datatype = CC_DATA_INT8;
        ccReduceOp_t op = CCE_RED_OP_SUM;
        rtStream_t stream = NULL;
        int i;

        memset(dst, 0, 10 * sizeof(char));

        rtStreamCreate(&stream, 5);
        //stream = (rtStream_t)malloc(sizeof(rtStream_t));

        /*检查入参*/
        ret = ccVectorReduce(NULL, (void*)&src2, count, datatype, op, stream, &dst);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);
        ret = ccVectorReduce((void*)&src1, NULL, count, datatype, op, stream, &dst);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);
        ret = ccVectorReduce((void*)&src1, (void*)&src2, count, datatype, op, stream, NULL);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);

        /*datatype检查*/
        datatype = CC_DATA_RESERVED;
        ret = ccVectorReduce((void*)&src1, (void*)&src2, count, datatype, op, stream, &dst);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);

        /*op检查*/
        datatype = CC_DATA_INT8;
        op = CCE_RED_OP_RESERVED;
        ret = ccVectorReduce((void*)&src1, (void*)&src2, count, datatype, op, stream, &dst);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);

        /*结果检查*/
        datatype = CC_DATA_INT8;
        op = CCE_RED_OP_SUM;
        ret = ccVectorReduce((void*)&src1[0], (void*)&src2[0], count, datatype, op, stream, &dst[0]);
        EXPECT_EQ(ret, CC_STATUS_SUCCESS);

        rtStreamSynchronize(stream);

        for (i = 0; i < 10; i++)
        {
            EXPECT_EQ(dst[i], 4);
        }

        rtStreamDestroy(stream);

        //free(stream);

    }


    /**
    * @brief test ccVectorReduce
    */
    TEST_F(RuntimeTest, ccVectorReduce_float)
    {
        ccStatus_t ret = CC_STATUS_SUCCESS;
        float src1[10] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
        float src2[10] = {3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0};
        float dst[10] ;
        uint32_t count = 10;
        ccDataType_t datatype = CC_DATA_FLOAT;
        ccReduceOp_t op = CCE_RED_OP_SUM;
        rtStream_t stream = NULL;
        int i;

        memset(dst, 0.0, 10 * sizeof(float));
        //stream = (rtStream_t)malloc(sizeof(rtStream_t));
        rtStreamCreate(&stream, 5);

        /*检查入参*/
        ret = ccVectorReduce(NULL, (void*)&src2, count, datatype, op, stream, &dst);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);
        ret = ccVectorReduce((void*)&src1, NULL, count, datatype, op, stream, &dst);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);
        ret = ccVectorReduce((void*)&src1, (void*)&src2, count, datatype, op, stream, NULL);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);

        /*datatype检查*/
        datatype = CC_DATA_RESERVED;
        ret = ccVectorReduce((void*)&src1, (void*)&src2, count, datatype, op, stream, &dst);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);

        /*op检查*/
        datatype = CC_DATA_FLOAT;
        op = CCE_RED_OP_RESERVED;
        ret = ccVectorReduce((void*)&src1, (void*)&src2, count, datatype, op, stream, &dst);
        EXPECT_EQ(ret, CC_STATUS_RESERVED);

        /*结果检查*/
        datatype = CC_DATA_FLOAT;
        op = CCE_RED_OP_SUM;
        ret = ccVectorReduce((void*)&src1[0], (void*)&src2[0], count, datatype, op, stream, &dst[0]);
        EXPECT_EQ(ret, CC_STATUS_SUCCESS);

        rtStreamSynchronize(stream);

        for (i = 0; i < 10; i++)
        {
            EXPECT_EQ(dst[i], 4.0);
        }

        //free(stream);
        rtStreamDestroy(stream);

    }


}


