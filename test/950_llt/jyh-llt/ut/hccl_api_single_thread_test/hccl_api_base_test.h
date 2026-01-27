#pragma once

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <runtime/rt.h>
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "externalinput.h"

#include <nlohmann/json.hpp>


#define private public
#define protected public
#include "topoinfo_detect.h"
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "hccl_comm_pub.h"
#include "coll_batch_send_recv_executor.h"
#include "coll_reduce_scatter_v_executor.h"
#include "coll_all_gather_v_executor.h"
#include "hccl_network.h"
#include "preempt_port_manager.h"
#undef protected
#undef private

#include "common/src/topo/hccl_whitelist.h"
#include "profiling_manager.h"
#include <hccl/hccl.h>
#include "llt_hccl_stub_pub.h"
#include <iostream>
#include <fstream>
#include "../inc/hccl/base.h"
#include "../inc/hccl/hccl_ex.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "op_base.h"
#include "adapter_rts.h"
#include "param_check_pub.h"
#include "hcom_pub.h"
#include "comm_config_pub.h"
#include "kernel_tiling/kernel_tiling.h"
#include "exception_handler.h"
#include "plugin_runner_pub.h"
#include "task_exception_handler_pub.h"
#include "topoinfo_exchange_dispatcher.h"
#include "heartbeat.h"
#include "network/hccp_common.h"
#include <dirent.h>

using namespace std;

using namespace hccl;

// 1KB
#define HCCL_COM_DATA_SIZE 1024

// 300MB
#define HCCL_COM_BIG_DATA_SIZE (300 * 1024 *1024)

class BaseInit : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;
protected:
    char rankTableFileName[24];
    HcclComm comm;
    rtStream_t stream;
};

// 设置deviceID
void Ut_Device_Set(int devId);

// 根据rankTable，创建对应的配置文件名，用于通信域初始化
void Ut_Clusterinfo_File_Create(const char *filename, nlohmann::json rankTable);

// 销毁对应通信域
void Ut_Comm_Destroy(void* &comm);

void Ut_Comm_CheckRet_Then_Destroy(HcclResult ret, void* &comm);

void Ut_Comm_Create(void* &comm, int devId, const char *rankTableFile, int rankId);

void Ut_Buf_Create(s8* &buf, int len);

// 用于简单测试，只对buf进行均分
void Ut_BufV_Create(s8* &buf, int bufLen, u64* &counts, int countsLen, int c, u64* &displs, int displsLen, int d);

void Ut_Stream_Create(rtStream_t &stream, int priority);

void Ut_Stream_Synchronize(rtStream_t &stream);

void Ut_Stream_Destroy(rtStream_t &stream);

void Ut_Stream_SynchronizeAndDestroy(rtStream_t &stream);

void Ut_MultiServer_MOCK_And_Clusterinfo_File_Create(const char *filename, nlohmann::json rankTable);

void When_Need_HcclGetRootInfo(void);

#define UT_USE_RANK_TABLE_910_1SERVER_1RANK                                                         \
    do {                                                                                            \
        Ut_Clusterinfo_File_Create(rankTableFileName,                                               \
            rank_table_910_1server_1rank);                                                          \
    } while (0)

#define UT_USE_RANK_TABLE_910_1SERVER_2RANK                                                         \
    do {                                                                                            \
        MOCKER_CPP(&TransportManager::Alloc)                                                        \
        .stubs()                                                                                    \
        .will(returnValue(HCCL_SUCCESS));                                                           \
        MOCKER_CPP(&HcclCommunicator::ExecOp)                                                       \
        .stubs()                                                                                    \
        .with(any())                                                                                \
        .will(returnValue(HCCL_SUCCESS));                                                           \
        Ut_Clusterinfo_File_Create(rankTableFileName,                                               \
            rank_table_910_1server_2rank);                                                          \
    } while (0)

#define UT_USE_RANK_TABLE_910_2SERVER_4RANK                                                         \
    do {                                                                                            \
        Ut_MultiServer_MOCK_And_Clusterinfo_File_Create(rankTableFileName,                          \
            rank_table_910_2server_4rank);                                                          \
    } while (0)

#define UT_USE_1SERVER_1RANK_AS_DEFAULT                                                             \
    do {                                                                                            \
        auto* info = ::testing::UnitTest::GetInstance()->current_test_info();                       \
        if (info) {                                                                                 \
            std::string name = info->name();                                                        \
            if (name.find("1Server2Rank") != std::string::npos) {                                   \
                UT_USE_RANK_TABLE_910_1SERVER_2RANK;                                                \
            } else if (name.find("2Server4Rank") != std::string::npos) {                            \
                UT_USE_RANK_TABLE_910_2SERVER_4RANK;                                                \
            } else {                                                                                \
                UT_USE_RANK_TABLE_910_1SERVER_1RANK;                                                \
            }                                                                                       \
        } else {                                                                                    \
            UT_USE_RANK_TABLE_910_1SERVER_1RANK;                                                    \
        }                                                                                           \
    } while (0)

#define UT_USE_1SERVER_2RANK_AS_DEFAULT                                                             \
    do {                                                                                            \
        auto* info = ::testing::UnitTest::GetInstance()->current_test_info();                       \
        if (info) {                                                                                 \
            std::string name = info->name();                                                        \
            if (name.find("2Server4Rank") != std::string::npos) {                                   \
                UT_USE_RANK_TABLE_910_2SERVER_4RANK;                                                \
            } else {                                                                                \
                UT_USE_RANK_TABLE_910_1SERVER_2RANK;                                                \
            }                                                                                       \
        } else {                                                                                    \
            UT_USE_RANK_TABLE_910_1SERVER_2RANK;                                                    \
        }                                                                                           \
    } while (0)

#define UT_SET_SENDBUF_RECVBUF_COUNT(sendSize, recvSize, countSize)                                 \
    do {                                                                                            \
        count = countSize;                                                                          \
        Ut_Buf_Create(sendBuf, sendSize);                                                           \
        Ut_Buf_Create(recvBuf, recvSize);                                                           \
    } while (0)

#define UT_UNSET_SENDBUF_RECVBUF()                                                                  \
    do {                                                                                            \
        sal_free(sendBuf);                                                                          \
        sal_free(recvBuf);                                                                          \
     } while (0)

#define UT_SET_SENDBUF_COUNT_RECVBUF_COUNT(sendSize, sendCountSize,                                 \
    recvSize, recvCountSize)                                                                        \
    do {                                                                                            \
        sendCount = sendCountSize;                                                                  \
        recvCount = recvCountSize;                                                                  \
        Ut_Buf_Create(sendBuf, sendSize);                                                           \
        Ut_Buf_Create(recvBuf, recvSize);                                                           \
    } while (0)

#define UT_SET_SENDBUF_COUNT(sendSize, countSize)                                                   \
    do {                                                                                            \
        count = countSize;                                                                          \
        Ut_Buf_Create(sendBuf, sendSize);                                                           \
    } while (0)

#define UT_UNSET_SENDBUF()                                                                          \
    do {                                                                                            \
        sal_free(sendBuf);                                                                          \
    } while (0)

#define UT_SET_RECVBUF_COUNT(recvSize, countSize)                                                   \
    do {                                                                                            \
        count = countSize;                                                                          \
        Ut_Buf_Create(recvBuf, recvSize);                                                           \
    } while (0)

#define UT_UNSET_RECVBUF()                                                                          \
    do {                                                                                            \
        sal_free(recvBuf);                                                                          \
    } while (0)

#define UT_SET_SENDBUFV_RECVBUF_COUNT(sendSize, sendCountsSize, sendCountsValue,                    \
    sendDisplsSize, sendDisplsValue, recvSize, recvCountSize)                                       \
    do {                                                                                            \
        recvCount = recvCountSize;                                                                  \
        Ut_BufV_Create(sendBuf, sendSize,                                                           \
                    sendCounts, sendCountsSize, sendCountsValue,                                    \
                    sendDispls, sendDisplsSize, sendDisplsValue);                                   \
        Ut_Buf_Create(recvBuf, recvSize);                                                           \
    } while (0)

#define UT_UNSET_SENDBUFV_RECVBUF()                                                                 \
    do {                                                                                            \
        sal_free(sendBuf);                                                                          \
        sal_free(sendCounts);                                                                       \
        sal_free(sendDispls);                                                                       \
        sal_free(recvBuf);                                                                          \
    } while (0)

#define UT_SET_SENDBUF_COUNT_RECVBUFV(sendSize, sendCountSize, recvSize,                            \
    recvCountsSize, recvCountsValue, recvDisplsSize, recvDisplsValue)                               \
    do {                                                                                            \
        sendCount = sendCountSize;                                                                  \
        Ut_Buf_Create(sendBuf, sendSize);                                                           \
        Ut_BufV_Create(recvBuf, recvSize,                                                           \
                    recvCounts, recvCountsSize, recvCountsValue,                                    \
                    recvDispls, recvDisplsSize, recvDisplsValue);                                   \
    } while (0)

#define UT_UNSET_SENDBUF_RECVBUFV()                                                                 \
    do {                                                                                            \
        sal_free(sendBuf);                                                                          \
        sal_free(recvBuf);                                                                          \
        sal_free(recvCounts);                                                                       \
        sal_free(recvDispls);                                                                       \
    } while (0)

#define UT_SET_SENDBUFV_RECVBUFV(sendSize, sendCountsSize, sendCountsValue, sendDisplsSize,         \
    sendDisplsValue, recvSize, recvCountsSize, recvCountsValue, recvDisplsSize, recvDisplsValue)    \
    do {                                                                                            \
        Ut_BufV_Create(sendBuf, sendSize, sendCounts, sendCountsSize, sendCountsValue,              \
            sendDispls, sendDisplsSize, sendDisplsValue);                                           \
        Ut_BufV_Create(recvBuf, recvSize, recvCounts, recvCountsSize, recvCountsValue,              \
            recvDispls, recvDisplsSize, recvDisplsValue);                                           \
    } while (0)

#define UT_UNSET_SENDBUFV_RECVBUFV()                                                                \
    do {                                                                                            \
        sal_free(sendBuf);                                                                          \
        sal_free(sendCounts);                                                                       \
        sal_free(sendDispls);                                                                       \
        sal_free(recvBuf);                                                                          \
        sal_free(recvCounts);                                                                       \
        sal_free(recvDispls);                                                                       \
    } while (0)

#define UT_COMM_CREATE_DEFAULT(comm)                                                                \
    do {                                                                                            \
        Ut_Comm_Create(comm, 0, rankTableFileName, 0);                                              \
    } while (0)

#define UT_STREAM_CREATE_DEFAULT(stream)                                                            \
    do {                                                                                            \
        Ut_Stream_Create(stream, 0);                                                                \
    } while (0)

#define UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, Stream)               \
    do {                                                                                            \
        UT_UNSET_SENDBUF_RECVBUF();                                                                 \
        Ut_Stream_SynchronizeAndDestroy(stream);                                                    \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_SENDBUF_RECVBUF_COMM_STREAM(comm, Stream)                                          \
    do {                                                                                            \
        UT_UNSET_SENDBUF_RECVBUF();                                                                 \
        Ut_Stream_Destroy(stream);                                                                  \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_SENDBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, Stream)                       \
    do {                                                                                            \
        UT_UNSET_SENDBUF();                                                                         \
        Ut_Stream_SynchronizeAndDestroy(stream);                                                    \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_SENDBUF_COMM_STREAM(comm, Stream)                                                  \
    do {                                                                                            \
        UT_UNSET_SENDBUF();                                                                         \
        Ut_Stream_Destroy(stream);                                                                  \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, Stream)                       \
    do {                                                                                            \
        UT_UNSET_RECVBUF();                                                                         \
        Ut_Stream_SynchronizeAndDestroy(stream);                                                    \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_RECVBUF_COMM_STREAM(comm, Stream)                                                  \
    do {                                                                                            \
        UT_UNSET_RECVBUF();                                                                         \
        Ut_Stream_Destroy(stream);                                                                  \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_SENDBUFV_RECVBUF_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, Stream)              \
    do {                                                                                            \
        UT_UNSET_SENDBUFV_RECVBUF();                                                                \
        Ut_Stream_SynchronizeAndDestroy(stream);                                                    \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_SENDBUFV_RECVBUF_COMM_STREAM(comm, Stream)                                         \
    do {                                                                                            \
        UT_UNSET_SENDBUFV_RECVBUF();                                                                \
        Ut_Stream_Destroy(stream);                                                                  \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, Stream)              \
    do {                                                                                            \
        UT_UNSET_SENDBUF_RECVBUFV();                                                                \
        Ut_Stream_SynchronizeAndDestroy(stream);                                                    \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_SENDBUF_RECVBUFV_COMM_STREAM(comm, Stream)                                         \
    do {                                                                                            \
        UT_UNSET_SENDBUF_RECVBUFV();                                                                \
        Ut_Stream_Destroy(stream);                                                                  \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, Stream)             \
    do {                                                                                            \
        UT_UNSET_SENDBUFV_RECVBUFV();                                                               \
        Ut_Stream_SynchronizeAndDestroy(stream);                                                    \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_SENDBUFV_RECVBUFV_COMM_STREAM(comm, Stream)                                        \
    do {                                                                                            \
        UT_UNSET_SENDBUFV_RECVBUFV();                                                               \
        Ut_Stream_Destroy(stream);                                                                  \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_COMM_STREAM_WITHSTREAMSYNCHRONIZEFIRST(comm, Stream)                               \
    do {                                                                                            \
        Ut_Stream_SynchronizeAndDestroy(stream);                                                    \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)

#define UT_UNSET_COMM_STREAM(comm, Stream)                                                          \
    do {                                                                                            \
        Ut_Stream_Destroy(stream);                                                                  \
        Ut_Comm_Destroy(comm);                                                                      \
    } while (0)
