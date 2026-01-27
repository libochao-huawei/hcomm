//
// Created by l00642779 on 1/6/24.
//

#ifndef HCCL_V2_STEST_ST_CTX_H
#define HCCL_V2_STEST_ST_CTX_H

#include "../hccl_st_situation.h"
#include "hccl_communicator.h"
// situation是公用的配置， context是执行起来之后各个线程拿到的上下文
struct ThreadContext {
    Situation situation;
    int serverId;
    int deviceId;
    int myRank;
    std::string commId;
    void* sendBuf;
    void* recvBuf;
    void* expectedResBuf;
    Hccl::HcclCommunicator* comm;

    virtual ~ThreadContext();
};

ThreadContext* GetCurrentThreadContext();

void SetCurrentThreadContext(ThreadContext* ctx);




#endif //HCCL_V2_STEST_ST_CTX_H
