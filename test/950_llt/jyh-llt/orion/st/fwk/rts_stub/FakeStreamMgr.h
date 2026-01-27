
#ifndef HCCL_V2_STEST_FAKESTREAMMGR_H
#define HCCL_V2_STEST_FAKESTREAMMGR_H

#include <mutex>
#include <atomic>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include "rts_stub.h"

enum class FakeSqeType { NOTIFY_WAIT, NOTIFY_RECORD, SDMA_REDUCE, MEM_CPY };

struct FakeSqe {
    FakeSqeType    type;
    int            notifyId;
    void          *dst;
    const void    *src;
    uint64_t       count; // 代表数据实际长度，并不是 dataSize;
    rtDataType_t   dataType;
    rtRecudeKind_t reduceOp;

    int srcRank{-1};
    int dstRank{-1};
};

class FakeNotifyMgr {
public:
    int *CreateNotify(int rank);
    int  GetRank(int notifyId);
    bool Record(int notifyId);
    bool Wait(int notifyId);
    void DestroyNotify(int *notifyId);

    virtual ~FakeNotifyMgr();

private:
    std::mutex lmutex;

    std::atomic_int notifyIdGen{0};
    std::set<int *> notifyIds;

    std::map<int, int> notifyStatusMap;
    std::map<int, int> notifyRankMap;
};

class FakeStreamMgr {
public:
    int *CreateStream(int rank);
    int  GetRank(int streamId);
    void Sync(int streamId);
    void Append(int streamId, FakeSqe sqe);
    void DestroyStream(int *streamId);

    FakeNotifyMgr *GetFakeNotifyMgr();

    virtual ~FakeStreamMgr();

private:
    std::mutex         lmutex;
    std::atomic_int    streamIdGen{0};
    std::set<int *>    streamIds;
    std::map<int, int> streamIdRankMap;

    std::map<int, std::vector<FakeSqe>> stores;
    std::unique_ptr<FakeNotifyMgr>      fakeNotifyMgr = std::make_unique<FakeNotifyMgr>();

    bool HasSqe();
};

#endif // HCCL_V2_STEST_FAKESTREAMMGR_H
