#include "typical/interface_hccl.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
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
    // 申请MR_COUNT块内存并分别注册，后续SGE将引用不同MR的内存
    void *mrBufs[MR_COUNT] = {};
    AscendMrAttr mrAttrs[MR_COUNT] = {};

    for (int i = 0; i < MR_COUNT; i++) {
        CHK_RET(hcclAllocWindowMem(&mrBufs[i], BUF_SIZE));
        mrAttrs[i].addr = reinterpret_cast<uint64_t>(mrBufs[i]);
        mrAttrs[i].size = BUF_SIZE;
        CHK_RET(hcclRdmaMemRegister(&mrAttrs[i]));
        printf("[INFO] MR[%d] registered: addr=0x%lx, size=%lu, lkey=%u, rkey=%u\n",
            i, mrAttrs[i].addr, mrAttrs[i].size, mrAttrs[i].lkey, mrAttrs[i].rkey);
    }

    // ========== 2. 创建CQ ==========
    AscendCQInfo sendCq = {};
    sendCq.cqDepth = CQ_DEPTH;
    CHK_RET(hcclCreateAscendCQ(&sendCq));
    printf("[INFO] Send CQ created: cqn=%u, cqDepth=%u\n", sendCq.cqn, sendCq.cqDepth);

    AscendCQInfo recvCq = {};
    recvCq.cqDepth = CQ_DEPTH;
    CHK_RET(hcclCreateAscendCQ(&recvCq));
    printf("[INFO] Recv CQ created: cqn=%u, cqDepth=%u\n", recvCq.cqn, recvCq.cqDepth);

    // ========== 3. 创建QP ==========
    AscendVerbsQPInfo qpInfo = {};
    qpInfo.sCqn = sendCq.cqn;
    qpInfo.rCqn = recvCq.cqn;
    qpInfo.srq = 0;
    qpInfo.cap.maxSendWr = 256;
    qpInfo.cap.maxRecvWr = 256;
    qpInfo.cap.maxSendSge = 4;
    qpInfo.cap.maxRecvSge = 4;
    qpInfo.cap.maxInlineData = 64;
    qpInfo.qp_type = IBV_QPT_RC;
    qpInfo.sqSigAll = 0;
    CHK_RET(hcclCreateAscendQPWithCQWithAttr(&sendCq, &recvCq, &qpInfo));
    printf("[INFO] QP created: qpn=%u\n", qpInfo.qpn);

    // 构造AscendQPInfo用于ModifyQp/PostSend
    AscendQPInfo localQpInfo = {};
    localQpInfo.qpn = qpInfo.qpn;

    // ========== 4. ModifyQp ==========
    // 实际场景中remoteQpInfo来自对端交换，此处用本地QP模拟
    AscendQPInfo remoteQpInfo = localQpInfo;
    AscendQPQos qpQos = {};
    qpQos.tc = 4;
    qpQos.sl = 1;
    CHK_RET(hcclModifyAscendQPEx(&localQpInfo, &remoteQpInfo, &qpQos));
    printf("[INFO] QP modified to RTS: local_qpn=%u, remote_qpn=%u, tc=%u, sl=%u\n",
        localQpInfo.qpn, remoteQpInfo.qpn, qpQos.tc, qpQos.sl);

    // ========== 5. PostSend ==========
    // 下发WR_COUNT个WR，每个WR包含SGE_PER_WR个SGE
    // 关键：每个WR中的SGE引用不同MR的内存区域
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

        const char *opcodeStr[] = {"RDMA_WRITE", "RDMA_WRITE_WITH_IMM", "SEND", "RDMA_READ"};
        printf("[INFO] WR[%d]: wrId=%lu, numSge=%d, opcode=%s, signaled=%s\n",
            i, sendWrs[i].wrId, sendWrs[i].numSge, opcodeStr[i],
            (sendWrs[i].sendFlags & IBV_SEND_SIGNALED) ? "yes" : "no");
        if (wrOpcodes[i] == IBV_WR_RDMA_WRITE_WITH_IMM) {
            printf("       immData=0x%x\n", sendWrs[i].immData);
        }
        for (int j = 0; j < SGE_PER_WR; j++) {
            printf("       SGE[%d]: addr=0x%lx, len=%u, lkey=%u (from MR[%d])\n",
                j, wrSgeLists[i][j].addr, wrSgeLists[i][j].len, wrSgeLists[i][j].lkey, j % MR_COUNT);
        }
    }

    struct AscendSendWr *badWr = nullptr;
    CHK_RET(hcclAscendPostSend(&localQpInfo, &sendWrs[0], &badWr));
    printf("[INFO] PostSend completed: %d WRs posted\n", WR_COUNT);

    // ========== 6. PollCq ==========
    const uint32_t POLL_COUNT = WR_COUNT;
    struct AscendWc wcList[POLL_COUNT] = {};
    uint32_t totalCompleted = 0;

    while (totalCompleted < POLL_COUNT) {
        uint32_t remain = POLL_COUNT - totalCompleted;
        uint32_t polledNum = 0;
        HcclResult ret = hcclPollAscendCQ(&sendCq, remain, &polledNum, &wcList[totalCompleted]);
        if (ret == HCCL_SUCCESS) {
            for (uint32_t i = totalCompleted; i < totalCompleted + polledNum; i++) {
                if (wcList[i].status == IBV_WC_SUCCESS) {
                    printf("[INFO] CQE completed: wrId=%lu, opcode=%d, byteLen=%u\n",
                        wcList[i].wrId, wcList[i].opcode, wcList[i].byteLen);
                } else {
                    printf("[ERROR] CQE error: wrId=%lu, status=%d\n", wcList[i].wrId, wcList[i].status);
                }
            }
            totalCompleted += polledNum;
        }
    }
    printf("[INFO] PollCq completed: %u CQEs polled\n", totalCompleted);

    // ========== 清理 ==========
    CHK_RET(hcclDestroyAscendQP(&localQpInfo));
    for (int i = 0; i < MR_COUNT; i++) {
        CHK_RET(hcclRdmaMemDeRegister(&mrAttrs[i]));
        CHK_RET(hcclFreeWindowMem(mrBufs[i]));
    }
    CHK_RET(hcclAscendRdmaDeInit());

    printf("[INFO] RDMA demo finished\n");
    return 0;
}
