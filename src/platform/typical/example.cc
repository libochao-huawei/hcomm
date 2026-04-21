#include "interface_hccl.h"
#include "typical/interface_hccl.h"
#include <cstdint>
#include <vector>

#define MR_COUNT 3
#define WR_COUNT 4
#define SGE_PER_WR 3
#define BUF_SIZE 4096
#define CQ_DEPTH 256

int main()
{
    // 设备初始化
    aclInit(NULL);
    aclrtSetDevice(0);

    // RDMA初始化
    CHK_RET(hcclAscendRdmaInit());

    // ========== 1. 注册Mr ==========
    void *mrBufs[MR_COUNT] = {};
    AscendMrAttr mrAttrs[MR_COUNT] = {};

    for (int i = 0; i < MR_COUNT; i++) {
        CHK_RET(hcclAllocWindowMem(&mrBufs[i], BUF_SIZE));
        mrAttrs[i].addr = reinterpret_cast<uint64_t>(mrBufs[i]);
        mrAttrs[i].size = BUF_SIZE;
        aclrtHostRegister(&mrAttrs[i].addr, mrAttrs[i].size, );
        CHK_RET(hcclRdmaMemRegister(&mrAttrs[i]));
    }

    // ========== 2. 创建CQ ==========
    AscendCQInfo sendCq = {};
    sendCq.cqDepth = CQ_DEPTH;
    CHK_RET(hcclCreateAscendCQWithAttr(&sendCq));

    AscendCQInfo recvCq = {};
    recvCq.cqDepth = CQ_DEPTH;
    CHK_RET(hcclCreateAscendCQWithAttr(&recvCq));

    // ========== 3. 创建QP ==========
    AscendVerbsQPInfo qpInfo = {};
    qpInfo.sCqn = sendCq.cqn;
    qpInfo.rCqn = recvCq.cqn;
    qpInfo.srq = 0;
    qpInfo.cap.maxSendWr = 512;
    qpInfo.cap.maxRecvWr = 128;
    qpInfo.cap.maxSendSge = 4;
    qpInfo.cap.maxRecvSge = 4;
    qpInfo.cap.maxInlineData = 64;
    qpInfo.qp_type = IBV_QPT_RC;
    qpInfo.sqSigAll = 0;
    CHK_RET(hcclCreateAscendQPWithCQWithAttr(&sendCq, &recvCq, &qpInfo));

    // ========== 4. ModifyQp ==========
    AscendVerbsQPInfo remoteQpInfo = qpInfo;
    AscendQPQos qpQos = {};
    qpQos.tc = 4;
    qpQos.sl = 1;
    CHK_RET(hcclModifyAscendVerbsQPEx(&qpInfo, &remoteQpInfo, &qpQos));

    // ========== 5. PostSend ==========
    std::vector<std::vector<AscendSge>> wrSgeLists(WR_COUNT);
    std::vector<AscendSendWr> sendWrs(WR_COUNT);

    static const enum AscendWrOpcode wrOpcodes[WR_COUNT] = {
        IBV_WR_RDMA_WRITE,
        IBV_WR_RDMA_WRITE_WITH_IMM,
        IBV_WR_SEND,
        IBV_WR_RDMA_READ
    };

    for (int i = 0; i < WR_COUNT; i++) {
        wrSgeLists[i].resize(SGE_PER_WR);
        for (int j = 0; j < SGE_PER_WR; j++) {
            int mrIdx = j % MR_COUNT;
            uint64_t offset = i * 128;
            wrSgeLists[i][j].addr = mrAttrs[mrIdx].addr + offset;
            wrSgeLists[i][j].len = 128;
            wrSgeLists[i][j].lkey = mrAttrs[mrIdx].lkey;
        }

        sendWrs[i].wrId = i;
        sendWrs[i].next = (i < WR_COUNT - 1) ? &sendWrs[i + 1] : nullptr;
        sendWrs[i].sgList = wrSgeLists[i].data();
        sendWrs[i].numSge = SGE_PER_WR;
        sendWrs[i].opcode = wrOpcodes[i];
        sendWrs[i].sendFlags = (i == WR_COUNT - 1) ? IBV_SEND_SIGNALED : static_cast<AscendSendFlags>(0);
        sendWrs[i].immData = (wrOpcodes[i] == IBV_WR_RDMA_WRITE_WITH_IMM) ? 0x1234 : 0;
        sendWrs[i].wr.rdma.remoteAddr = mrAttrs[0].addr;
        sendWrs[i].wr.rdma.rkey = mrAttrs[0].rkey;
    }

    struct AscendSendWr *badWr = nullptr;
    CHK_RET(hcclAscendPostSend(&qpInfo, &sendWrs[0], &badWr));

    // ========== 6. PollCq ==========
    const uint32_t POLL_COUNT = WR_COUNT;
    struct AscendWc wcList[POLL_COUNT] = {};
    uint32_t totalCompleted = 0;

    while (totalCompleted < POLL_COUNT) {
        uint32_t remain = POLL_COUNT - totalCompleted;
        uint32_t polledNum = 0;
        HcclResult ret = hcclPollAscendCQ(&sendCq, remain, &polledNum, &wcList[totalCompleted]);
        if (ret == HCCL_SUCCESS) {
            totalCompleted += polledNum;
        }
    }

    // ========== 清理 ==========
    CHK_RET(hcclDestroyAscendVerbsQP(&qpInfo));
    CHK_RET(hcclDestroyAscendCQ(&sendCq));
    CHK_RET(hcclDestroyAscendCQ(&recvCq));
    for (int i = 0; i < MR_COUNT; i++) {
        CHK_RET(hcclRdmaMemDeRegister(&mrAttrs[i]));
        CHK_RET(hcclFreeWindowMem(mrBufs[i]));
    }
    CHK_RET(hcclAscendRdmaDeInit());

    return 0;
}
