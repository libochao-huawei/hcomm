//
// Created by l00642779 on 1/6/24.
//

#include "st_ctx.h"
#include "../rts_stub/rts_stub.h"


thread_local ThreadContext* ctx;


ThreadContext* GetCurrentThreadContext() {
    return ctx;
}

void SetCurrentThreadContext(ThreadContext* newCtx) {
    ctx = newCtx;
}

ThreadContext::~ThreadContext() {
    delete comm;
    aclrtFree(sendBuf);
    aclrtFree(recvBuf);
    aclrtFree(expectedResBuf);
}
