/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <getopt.h>
#include <memory>
#include <mutex>
#include <fstream>
#include <map>
#include <functional>
#include <unistd.h>

// Default to mock unless overridden
#ifndef MOCK_HCCL
#define MOCK_HCCL 0
#endif

#if !MOCK_HCCL
// Real headers (NPU environment). Adjust include paths if needed.
#include "hccl/hccl_comm.h"
#include "hccl/hccl_inner.h"
#include "hccl/hccl_types.h"
#include "hcomm_primitives.h"
#include "hccl_res.h"
#include "hccl_comm.h"
#include "acl/acl.h"
#endif

#define DEBUG       0x0
#define INFO        0x1
#define WARNING     0x2
#define ERROR       0x3
#define LOG_LEVEL   DEBUG

#define LOG(log_level, format, arg...) do {                                             \
    if (log_level == ERROR) {                                                           \
        printf("[ERROR] [%s:%d] [%u] ", __FILE__, __LINE__, getpid());                  \
        printf(format, ##arg);                                                          \
        printf("\n");                                                                   \
    } else if (log_level >= LOG_LEVEL) {                                                \
        printf("[%s] [%s:%d] [%u] ", #log_level, __FILE__, __LINE__, getpid());         \
        printf(format, ##arg);                                                          \
        printf("\n");                                                                   \
    }                                                                                   \
} while(0)

#define CHK_RET(call)                                 \
    do {                                              \
        bool hcclRet = call;                        \
        if (hcclRet == true) {                    \
            LOG(ERROR, "[%s]call trace: hcclRet -> %d", __func__, hcclRet); \
            return hcclRet;                               \
        }                                             \
    } while (0)

// ---------------------------------------------------------------------------
// Minimal HCCL types/prototypes used by this test (when mocking).
// If MOCK_HCCL=0 we'll link to real library so these prototypes must match.
// ---------------------------------------------------------------------------
extern "C" {
#if MOCK_HCCL
typedef int32_t HcclResult;
typedef void *HcclComm;
typedef uint64_t ThreadHandle;
typedef uint64_t ChannelHandle;
typedef uint64_t NotifyHandle;

typedef enum {
    COMM_ENGINE_RESERVED = -1,   ///< 保留的通信引擎
    COMM_ENGINE_HOSTCPU = 0,     ///< HOST CPU引擎
    COMM_ENGINE_HOSTCPU_TS = 1,  ///< HOST CPU TS引擎
    COMM_ENGINE_AICPU = 2,       ///< AICPU引擎
    COMM_ENGINE_AICPU_TS = 3,    ///< AICPU TS引擎
    COMM_ENGINE_AIV = 4,         ///< AIV引擎
    COMM_ENGINE_CCU = 5,         ///< CCU引擎
} CommEngine;

typedef enum {
    NOTIFY_TYPE_RESERVED = -1,
    NOTIFY_TYPE_RTS_NOTIFY = 0,
    NOTIFY_TYPE_RTS_EVENT = 1,
    NOTIFY_TYPE_DEVICE_MEM = 2,
} NotifyType;

struct HcclCommConfig {
    // minimal fields used in test
    uint32_t hcclBufferSize;
    char hcclCommName[64];
    int32_t commEngine;             ///< 通信引擎（0: HOST CPU；1: HOST CPU TS；...)（参考CommEngine，从hcclOpExpansionMode变更）
    uint32_t threadNum;             ///< thread数量（新增）
    uint32_t notifyNumPerThread;    ///< 每个thread的notify数量（新增）
};
struct ChannelDesc {
    uint32_t peerRank;
    uint64_t size;
    // additional mock fields
    uint32_t priority;
    uint32_t bandwidth;
};

typedef enum {
    HCCL_MEM_TYPE_DEVICE, ///< 设备侧内存（如NPU等）
    HCCL_MEM_TYPE_HOST,   ///< 主机侧内存
    HCCL_MEM_TYPE_NUM     ///< 内存类型数量
} HcclMemType;

typedef struct {
    HcclMemType type; ///< 缓存物理位置类型，参见HcclMemType
    void *addr;       ///< 缓存地址
    uint64_t size;    ///< 缓存区域字节数
} CommBuffer;


#else
#endif

HcclResult __attribute__((weak))
    HcclCommInitClusterInfoConfig(const char *clusterInfo, uint32_t rank, HcclCommConfig *config, HcclComm *comm);
HcclResult __attribute__((weak)) HcclCommDestroy(HcclComm comm);
    void __attribute__((weak)) HcclCommConfigInit(HcclCommConfig *config);
HcclResult __attribute__((weak)) HcclAllocThreadRes(
    HcclComm comm, CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, ThreadHandle *thread);
HcclResult __attribute__((weak)) CommChannelCreate(HcclComm comm, const char *channelTag, CommEngine engine,
    const ChannelDesc *channelDescList, uint32_t listNum, ChannelHandle *channelList);
HcclResult __attribute__((weak))
    HcommWriteOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len);
HcclResult __attribute__((weak))
    HcommReadOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len);
HcclResult __attribute__((weak)) HcclAllocNotify(HcclComm comm, CommEngine commEngine, NotifyType notifyType,
    uint32_t notifyNum, NotifyHandle **notifyHandleList);
HcclResult __attribute__((weak)) HcommFreeNotify(HcclComm comm, uint32_t notifyNum, NotifyHandle *notifyHandleList);
HcclResult __attribute__((weak)) HcclGetHcclBuffer(HcclComm comm, CommBuffer *buffer);
HcclResult __attribute__((weak)) HcclChannelGetHcclBuffer(HcclComm comm, ChannelHandle channel, CommBuffer *buffer);


}  // extern "C"

#if MOCK_HCCL
// --------------------------- Mock HCCL impl ------------------------------
#include <map>
#include <atomic>
static std::mutex g_mock_mtx;
struct MockComm {
    uint32_t rankCount;
    uint32_t myRank;
    std::string name;
};
struct MockThreadRes {
    uint32_t threads;
};
struct MockChannel {
    std::string tag;
    uint32_t peer;
    uint64_t size;
};
struct MockNotifys {
    uint32_t notifyNum;
};

HcclResult HcclCommInitClusterInfoConfig(const char *clusterInfo, uint32_t rank, HcclCommConfig *config, HcclComm *comm)
{
    std::lock_guard<std::mutex> lk(g_mock_mtx);
    // try parse clusterInfo as a file path (very small JSON-ish parsing)
    uint32_t rankCount = 0;
    if (clusterInfo) {
        std::ifstream ifs(clusterInfo);
        if (ifs.is_open()) {
            std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            // crude count of "rank_id" occurences
            size_t pos = 0;
            while ((pos = s.find("rank_id", pos)) != std::string::npos) {
                rankCount++;
                pos += 7;
            }
        }
    }
    if (rankCount == 0)
        rankCount = 1;  // fallback
    MockComm *c = new MockComm();
    c->rankCount = rankCount;
    c->myRank = rank;
    c->name = std::string(config ? config->hcclCommName : "mock_comm");
    *comm = c;
    std::cout << "[MOCK] HcclCommInitClusterInfoConfig rank=" << rank << " ranks=" << rankCount << " name=" << c->name
              << std::endl;
    return 0;
}
HcclResult HcclCommDestroy(HcclComm comm)
{
    std::lock_guard<std::mutex> lk(g_mock_mtx);
    if (!comm)
        return 0;
    delete static_cast<MockComm *>(comm);
    std::cout << "[MOCK] HcclCommDestroy\n";
    return 0;
}
inline void HcclCommConfigInit(HcclCommConfig *config)
{
    *config = HcclCommConfig();
}
HcclResult HcclAllocThreadRes(
    HcclComm comm, CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, ThreadHandle *thread)
{
    std::lock_guard<std::mutex> lk(g_mock_mtx);
    MockThreadRes *r = new MockThreadRes();
    r->threads = threadNum;
    *thread = reinterpret_cast<ThreadHandle>(r);
    std::cout << "[MOCK] HcclAllocThreadRes threads=" << threadNum << " notify=" << notifyNumPerThread << std::endl;
    return 0;
}
HcclResult CommChannelCreate(HcclComm comm, const char *channelTag, CommEngine engine,
    const ChannelDesc *channelDescList, uint32_t listNum, ChannelHandle *channelList)
{
    std::lock_guard<std::mutex> lk(g_mock_mtx);
    if (listNum == 0)
        return -1;
    for (uint32_t i = 0; i < listNum; i++) {
        MockChannel *ch = new MockChannel();
        ch->tag = (channelTag ? channelTag : "ch") + std::string(":") + std::to_string(i);
        ch->peer = channelDescList ? channelDescList[i].peerRank : 0;
        ch->size = channelDescList ? channelDescList[i].size : 0;
        channelList[i] = reinterpret_cast<ChannelHandle>(ch);
    }
    std::cout << "[MOCK] CommChannelCreate tag=" << (channelTag ? channelTag : "(null)") << " count=" << listNum
              << std::endl;
    return 0;
}

HcclResult HcommWriteOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    // copy if pointers valid
    if (dst && src && len > 0)
        memcpy(dst, src, (size_t)len);
    return 0;
}

HcclResult HcclAllocNotify(HcclComm comm, CommEngine commEngine, NotifyType notifyType,
    uint32_t notifyNum, NotifyHandle **notifyHandleList)
{
    std::lock_guard<std::mutex> lk(g_mock_mtx);
    *notifyHandleList = reinterpret_cast<NotifyHandle*>(malloc(sizeof(NotifyHandle) * notifyNum));
    for (uint32_t i = 0; i < notifyNum; ++i) {
        (*notifyHandleList)[i] = reinterpret_cast<NotifyHandle>(new MockNotifys());
    }

    std::cout << "[MOCK] HcclAllocNotify notifyNum=" << notifyNum << " notifyType="
        << static_cast<int32_t>(notifyType) << std::endl;
    return 0;
}

HcclResult HcommFreeNotify(HcclComm comm, uint32_t notifyNum, NotifyHandle *notifyHandleList)
{
    std::lock_guard<std::mutex> lk(g_mock_mtx);

    if (notifyHandleList == nullptr) {
        std::cerr << "[MOCK] HcommFreeNotify: notifyHandleList is null" << std::endl;
        return 0;
    }

    for (uint32_t i = 0; i < notifyNum; ++i) {
        if (notifyHandleList[i] != 0) {
            delete reinterpret_cast<MockNotifys*>(notifyHandleList[i]);
            notifyHandleList[i] = 0;
        }
    }

    free(notifyHandleList);
    std::cout << "[MOCK] HcommFreeNotify notifyNum=" << notifyNum << std::endl;
    return 0;
}

#endif  // MOCK_HCCL

// --------------------------- Device Buffer abstraction ---------------------
// Default host fallback allocator (posix_memalign). When using real ACL,
// we attempt to allocate device memory via aclrtMalloc when MOCK_HCCL=0.
struct DeviceBuffer {
    void *ptr = nullptr;
    uint64_t size = 0;
    const void *memHandle = nullptr;  // platform-specific (mock: nullptr)
};

static void *host_aligned_alloc(size_t size, size_t align = 4096)
{
    void *p = nullptr;
    if (posix_memalign(&p, align, size) != 0)
        return nullptr;
    return p;
}

DeviceBuffer allocate_device_buffer(uint64_t size, int device = 0)
{
    DeviceBuffer db;
#if MOCK_HCCL
    db.ptr = host_aligned_alloc((size_t)size);
    db.size = size;
    db.memHandle = nullptr;
    if (db.ptr) {
        memset(db.ptr, 0, (size_t)size);
    }
    LOG(DEBUG, "[MOCK]%s ptr[%p], size[%llu], memHandle[%p]", __func__, db.ptr, db.size, db.memHandle);
#else
    // Try to use ACL device malloc; if fails, fallback to host alloc
    void *devptr = nullptr;
    aclError ret = aclrtMalloc(&devptr, size, ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret == ACL_SUCCESS && devptr) {
        db.ptr = devptr;
        db.size = size;
        db.memHandle = devptr;  // often HCCL accepts device pointer as mem handle; if not, user must adapt
        LOG(DEBUG, "%s aclrtMalloc ptr[%p], size[%llu], memHandle[%p]", __func__, db.ptr, db.size, db.memHandle);
    } else {
        LOG(ERROR, "%s [WARN] aclrtMalloc failed, fallback to host malloc", __func__);
        db.ptr = host_aligned_alloc((size_t)size);
        db.size = size;
        db.memHandle = nullptr;
        if (db.ptr) {
            memset(db.ptr, 0, (size_t)size);
        }
        LOG(DEBUG, "%s host_aligned_alloc ptr[%p], size[%llu], memHandle[%p]", __func__, db.ptr, db.size, db.memHandle);
    }
#endif
    return db;
}

void free_device_buffer(DeviceBuffer &db)
{
    if (!db.ptr)
        return;
#if MOCK_HCCL
    free(db.ptr);
#else
    // try device free first
    aclError ret = aclrtFree(db.ptr);
    if (ret != ACL_SUCCESS) {
        // fallback
        free(db.ptr);
    }
#endif
    db.ptr = nullptr;
    db.size = 0;
    db.memHandle = nullptr;
}

// --------------------------- Utility: parse ranktable -----------------------
#include <sstream>
static std::vector<uint32_t> parse_rank_table_to_ranks(const std::string &path_or_json)
{
    std::vector<uint32_t> ranks;
    std::ifstream ifs(path_or_json);
    std::string content;
    if (ifs.is_open()) {
        content.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    } else {
        // maybe input is raw json string
        content = path_or_json;
    }
    size_t pos = 0;
    while ((pos = content.find("rank_id", pos)) != std::string::npos) {
        // attempt to find the quoted number after :
        size_t colon = content.find(':', pos);
        if (colon == std::string::npos)
            break;
        size_t quote1 = content.find_first_of("\"0123456789", colon + 1);
        if (quote1 == std::string::npos)
            break;
        // read digits
        size_t start = content.find_first_of("0123456789", colon + 1);
        if (start == std::string::npos)
            break;
        size_t end = start;
        while (end < content.size() && isdigit((unsigned char)content[end]))
            ++end;
        std::string num = content.substr(start, end - start);
        if (!num.empty()) {
            ranks.push_back((uint32_t)std::stoul(num));
        }
        pos = end;
    }
    if (ranks.empty()) {
        // fallback: assume single rank 0
        ranks.push_back(0);
    }
    return ranks;
}

// --------------------------- Config & HcclFwkTest class -----------------------
struct Config {
    std::string clusterInfo;  // path to ranktable or raw json
    uint32_t rank = 0;
    uint32_t peers = 1; // 每两个rank之间建立的channel数量
    std::string test = "resource";  // resource / pingpong / all
    uint32_t iters = 100;
    uint64_t msgSize = 64;
    uint32_t threadNum = 1;
    CommEngine commEngine = COMM_ENGINE_HOSTCPU;
};

// -------测试用例注册--------
using TestFunc = std::function<bool(const Config &)>;

static std::map<std::string, TestFunc> &GetTestRegistry()
{
    static std::map<std::string, TestFunc> registry;
    return registry;
}

#define REGISTER_HCCL_TEST(name, func)   \
    static bool _reg_##func = []() {     \
        GetTestRegistry()[#name] = func; \
        return true;                     \
    }();

static void ListAllTests()
{
    std::cout << "Available tests:" << std::endl;
    for (auto &kv : GetTestRegistry()) {
        std::cout << "  - " << kv.first << std::endl;
    }
}

// -------HcclFwkTest class--------
class HcclFwkTest {
public:
    HcclFwkTest(const Config &cfg) : cfg_(cfg), comm_(nullptr), threadRes_(reinterpret_cast<ThreadHandle>(nullptr))
    {}
    ~HcclFwkTest()
    {
        cleanup();
    }

    // init: ACL init (if real), set device, init HCCL comm, alloc thread resources
    bool init()
    {
#if !MOCK_HCCL
        // Initialize ACL runtime
        aclError aret = aclInit(nullptr);
        if (aret != ACL_SUCCESS) {
            std::cerr << "[ERROR] aclInit failed ret=" << aret << std::endl;
            return true;
        }
        // set device according to rank (this sample uses rank as device id)
        aret = aclrtSetDevice(static_cast<int>(cfg_.rank));
        if (aret != ACL_SUCCESS) {
            std::cerr << "[ERROR] aclrtSetDevice failed ret=" << aret << std::endl;
            // try finalize
            aclFinalize();
            return true;
        }
        std::cout << "[ACL] Initialized and set device " << cfg_.rank << std::endl;
#endif
        // Prepare HcclCommConfig
        HcclCommConfig config{};

        HcclCommConfigInit(&config);
        // Fill a few fields if exist
        config.hcclBufferSize = 64;
        config.commEngine = 0;
        config.threadNum = 8;
        config.notifyNumPerThread = 2;
        strncpy(config.hcclCommName, "hccl_fwk_test_comm", sizeof(config.hcclCommName) - 1);

        // call HcclCommInitClusterInfoConfig (mock or real)
        HcclResult r = HcclCommInitClusterInfoConfig(cfg_.clusterInfo.c_str(), cfg_.rank, &config, &comm_);
        if (r != 0 || comm_ == nullptr) {
            std::cerr << "[ERROR] HcclCommInitClusterInfoConfig failed rc=" << r << std::endl;
            return true;
        }
        std::cout << "[HCCL] Comm initialized\n";

        return false;
    }

    bool AllocThread(CommEngine commEngine)
    {
        // Alloc thread resource (optional, but often required to use HcommWriteOnThread)
        HcclResult r2 = HcclAllocThreadRes(comm_, /*engine=*/commEngine, 1, /*notify*/ 1, &threadRes_);
        if (r2 != 0 || threadRes_ == 0) {
            LOG(ERROR, "%s HcclAllocThreadRes fail, ret[%d]", __func__, r2);
            return true;
        } else {
            LOG(INFO, "%s HcclAllocThreadRes success, allocated threads[%u]", __func__, cfg_.threadNum);
        }

        return false;
    }

    bool AllocNotify(CommEngine commEngine, NotifyType notifyType, uint32_t notifyNum, NotifyHandle **notifyHandleList)
    {
        HcclResult ret = HcclAllocNotify(comm_, commEngine, notifyType, notifyNum, notifyHandleList);
        if (ret != 0 || *notifyHandleList == nullptr) {
            std::cerr << "[FAIL] HcclAllocNotify failed ret=" << ret << std::endl;
            return true;
        } else {
            std::cout << "[HCCL] Notify resources allocated Notifys=" << notifyNum << std::endl;
        }

        return false;
    }

    bool FreeNotify(uint32_t notifyNum, NotifyHandle *notifyHandleList)
    {
        HcclResult ret = HcommFreeNotify(comm_, notifyNum, notifyHandleList);
        if (ret != 0) {
            std::cerr << "[FAIL] HcommFreeNotify failed ret=" << ret << std::endl;
            return true;
        } else {
            std::cout << "[HCCL] Notify resources free Notifys=" << notifyNum << std::endl;
        }

        return false;
    }

    // setup channels and buffers: create one channel per peer and allocate buffers
    bool setup_channel_pair(CommEngine commEngine)
    {
        uint64_t buffSize = cfg_.msgSize + 128;
        uint32_t peers = cfg_.peers;
        buffers_.resize(peers);
        // alloc device mem
        for (uint32_t i = 0; i < peers; ++i) {
            buffers_[i] = allocate_device_buffer(buffSize, (int)cfg_.rank);
            if (!buffers_[i].ptr) {
                LOG(ERROR, "%s allocate_device_buffer failed for peer[%d]", __func__, i);
                return true;
            }
        }

        // build ChannelDesc list (use simple fields compatible with mock)
        std::vector<ChannelDesc> descs(peers);
#if MOCK_HCCL
        for (uint32_t i = 0; i < peers; i++) {
            descs[i].peerRank = i;
            descs[i].size = buffSize;
            descs[i].priority = 0;
            descs[i].bandwidth = 100;
        }
#else
        for (uint32_t i = 0; i < peers; i++) {
            memset(&descs[i], 0, sizeof(ChannelDesc));
            descs[i].remoteRank = cfg_.rank == 0 ? 1 : 0;
            descs[i].notifyNum = 3;
        }
#endif
        // create channel handles vector
        channels_.resize(peers, 0);

        // call CommChannelCreate
        HcclResult rc = CommChannelCreate(comm_, "p2p_channel", commEngine, descs.data(),
            static_cast<uint32_t>(descs.size()), channels_.data());
        if (rc != 0) {
            LOG(ERROR, "%s CommChannelCreate failed, ret[%d]", __func__, rc);
            return true;
        }
        LOG(INFO, "%s success, peers[%u]", __func__, peers);
        return false;
    }

    // small verification write if threadRes and channels available
    bool do_sample_write()
    {
        ChannelHandle &channel = channels_[0];
        CommBuffer localBuf;
        CHK_RET(HcclGetHcclBuffer(comm_, &localBuf));

        CommBuffer remoteBuf;
        CHK_RET(HcclChannelGetHcclBuffer(comm_, channel, &remoteBuf));

        uint64_t len = std::min(localBuf.size, remoteBuf.size);
        LOG(INFO, "%s localBuf: addr[%p] size[%llu], remoteBuf: addr[%p] size[%llu], len[%llu]",
            __func__, localBuf.addr, localBuf.size, remoteBuf.addr, remoteBuf.size, len);

        CHK_RET(HcommWriteOnThread(threadRes_, channel, remoteBuf.addr, localBuf.addr, len));
        LOG(INFO, "%s success, len[%u]", __func__, len);
        return false;
    }

    bool do_sample_read()
    {
        ChannelHandle &channel = channels_[0];
        CommBuffer localBuf;
        CHK_RET(HcclGetHcclBuffer(comm_, &localBuf));

        CommBuffer remoteBuf;
        CHK_RET(HcclChannelGetHcclBuffer(comm_, channel, &remoteBuf));

        uint64_t len = std::min(localBuf.size, remoteBuf.size);
        LOG(INFO, "%s localBuf: addr[%p] size[%llu], remoteBuf: addr[%p] size[%llu], len[%llu]",
            __func__, localBuf.addr, localBuf.size, remoteBuf.addr, remoteBuf.size, len);

        CHK_RET(HcommReadOnThread(threadRes_, channel, localBuf.addr, remoteBuf.addr, len));
        LOG(INFO, "%s success, len[%u]", __func__, len);
        return false;
    }

    void cleanup()
    {
        // free channels (mock might allocate channel handles)
#if MOCK_HCCL
        for (auto &ch : channels_) {
            if (ch)
                delete reinterpret_cast<void *>(ch);
            ch = 0;
        }
        if (threadRes_) {
            delete reinterpret_cast<void *>(threadRes_);
            threadRes_ = 0;
        }
#endif
        // free device buffers
        for (auto &b : buffers_)
            free_device_buffer(b);
        buffers_.clear();
        channels_.clear();

        if (comm_) {
            HcclCommDestroy(comm_);
            comm_ = nullptr;
        }

#if !MOCK_HCCL
        // finalize ACL
        aclError aret = aclFinalize();
        if (aret != ACL_SUCCESS) {
            std::cerr << "[WARN] aclFinalize returned " << aret << std::endl;
        } else {
            std::cout << "[ACL] Finalized\n";
        }
#endif
    }

private:
    Config cfg_;
    HcclComm comm_;
    ThreadHandle threadRes_;
    std::vector<ChannelHandle> channels_;
    std::vector<DeviceBuffer> buffers_;
};

// -------------------- 新增：测试用例实现与注册 (最少改动) --------------------

// test: comm init/destroy repeated
bool test_comminit(const Config &cfg)
{
    std::cout << "=== test_comminit ===\n";
    const int rounds = 3;
    for (int i = 0; i < rounds; ++i) {
        Config c = cfg;
        HcclFwkTest t(c);
        CHK_RET(t.init());
    }
    std::cout << "[PASS] test_comminit\n";
    return false;
}
REGISTER_HCCL_TEST(comminit, test_comminit);

// test: allocate thread resources with various thread counts
bool test_alloc_aicpu_thread(const Config &cfg)
{
    std::cout << "=== test_alloc_aicpu_thread ===\n";
    std::vector<uint32_t> tlist = {4, 4, 4};

    Config c = cfg;
    c.threadNum = 1;
    HcclFwkTest t(c);
    CHK_RET(t.init());

    for (auto tn : tlist) {
        CHK_RET(t.AllocThread(COMM_ENGINE_AICPU_TS));
        std::cout << "[INFO] alloc ok for threadNum=" << tn << std::endl;
    }
    std::cout << "[PASS] test_alloc_aicpu_thread\n";
    return false;
}
REGISTER_HCCL_TEST(alloc_aicpu_thread, test_alloc_aicpu_thread);

bool test_alloc_host_thread(const Config &cfg)
{
    std::cout << "=== alloc_host_thread ===\n";
    std::vector<uint32_t> tlist = {4, 4, 4};

    Config c = cfg;
    c.threadNum = 1;
    HcclFwkTest t(c);
    CHK_RET(t.init());

    for (auto tn : tlist) {
        CHK_RET(t.AllocThread(COMM_ENGINE_HOSTCPU_TS));

        std::cout << "[INFO] alloc ok for threadNum=" << tn << std::endl;
    }
    std::cout << "[PASS] alloc_host_thread\n";
    return false;
}
REGISTER_HCCL_TEST(alloc_host_thread, test_alloc_host_thread);

// test: allocate notify resources with various counts
bool test_alloc_aicpu_notify(const Config &cfg)
{
    std::cout << "=== test_alloc_aicpu_notify ===\n";
    std::vector<uint32_t> nlist = {4, 8, 16}; // 不同数量的 notify 测试
    Config c = cfg;
    HcclFwkTest t(c);
    CHK_RET(t.init());

    for (auto nn : nlist) {
        NotifyHandle *notifyList = nullptr;
        bool ret = t.AllocNotify(COMM_ENGINE_AICPU_TS, NOTIFY_TYPE_DEVICE_MEM, nn, &notifyList);
        if (ret || notifyList == nullptr) {
            std::cerr << "[FAIL] alloc notify failed num=" << nn << std::endl;
            return true;
        }
        std::cout << "[INFO] alloc notifyNum=" << nn << std::endl;
        // 模拟使用 notify ... 这里略
        ret = t.FreeNotify(nn, notifyList);
        if (ret) {
            std::cerr << "[FAIL] free notify failed num=" << nn << std::endl;
            return true;
        }
        std::cout << "[INFO] free notifyNum=" << nn << std::endl;
    }
    std::cout << "[PASS] test_alloc_aicpu_notify\n";
    return false;
}
REGISTER_HCCL_TEST(alloc_aicpu_notify, test_alloc_aicpu_notify);


// test: allocate host notify resources
bool test_alloc_host_notify(const Config &cfg)
{
    std::cout << "=== test_alloc_host_notify ===\n";
    std::vector<uint32_t> nlist = {2, 4, 8};

    Config c = cfg;
    HcclFwkTest t(c);
    CHK_RET(t.init());
    for (auto nn : nlist) {
        NotifyHandle *notifyList = nullptr;
        bool ret = t.AllocNotify(COMM_ENGINE_HOSTCPU_TS, NOTIFY_TYPE_RTS_NOTIFY, nn, &notifyList);
        if (ret || notifyList == nullptr) {
            std::cerr << "[FAIL] alloc host notify failed num=" << nn << std::endl;
            return true;
        }
        std::cout << "[INFO] alloc host notifyNum=" << nn << std::endl;
        ret = t.FreeNotify(nn, notifyList);
        if (ret) {
            std::cerr << "[FAIL] free host notify failed num=" << nn << std::endl;
            return true;
        }
        std::cout << "[INFO] free host notifyNum=" << nn << std::endl;
    }
    std::cout << "[PASS] test_alloc_host_notify\n";
    return false;
}
REGISTER_HCCL_TEST(alloc_host_notify, test_alloc_host_notify);

// test: 测试两卡CommChannelCreate, 需同时拉起两个进程
// rank_0进程: hccl_fwk_test --cluster_info test/hlt/ranktable-2p.json --rank 0 --test channel_create --engine 2
// rank_1进程: hccl_fwk_test --cluster_info test/hlt/ranktable-2p.json --rank 1 --test channel_create --engine 2
bool test_channel_create(const Config &cfg)
{
    LOG(INFO, "%s start", __func__);
    Config c = cfg;
    c.peers = 1;
    HcclFwkTest t(c);
    CHK_RET(t.init());
    CHK_RET(t.setup_channel_pair(cfg.commEngine));
    sleep(1); // tip：两个进程，建链完会自动销毁，没有等待对端完成建链的握手机制，暂时通过添加sleep来等待对端。如果等待时间内对端未完成建链，本端会销毁，导致对端建链失败
    LOG(INFO, "%s PASS", __func__);
    return false;
}
REGISTER_HCCL_TEST(channel_create, test_channel_create);

// test: 测试host展开下，两卡HcclAllocThreadRes+CommChannelCreate+Write+Read, 需同时拉起两个进程
bool test_host_write_read(const Config &cfg)
{
    LOG(INFO, "%s start", __func__);
    Config c = cfg;
    c.peers = 1;
    HcclFwkTest t(c);

    CHK_RET(t.init());

    CHK_RET(t.AllocThread(cfg.commEngine));

    CHK_RET(t.setup_channel_pair(cfg.commEngine));

    if (cfg.rank == 0) {
        CHK_RET(t.do_sample_write());
    } else {
        CHK_RET(t.do_sample_read());
    }
    sleep(1);
    LOG(INFO, "%s PASS", __func__);
    return false;
}
REGISTER_HCCL_TEST(host_write_read, test_host_write_read);

// test: stress create/destroy loops
bool test_stress(const Config &cfg)
{
    std::cout << "=== test_stress_create_destroy ===\n";
    int iters = 50;
    int fails = 0;
    for (int i = 0; i < iters; ++i) {
        Config c = cfg;
        c.peers = 1 + (i % std::max<uint32_t>(1, cfg.peers));
        HcclFwkTest t(c);
        if (t.init() || t.setup_channel_pair(COMM_ENGINE_HOSTCPU_TS)) {
            ++fails;
            std::cerr << "[WARN] iter=" << i << " failed\n";
        }
    }
    std::cout << "[RESULT] stress iters=" << iters << " fail_count=" << fails << "\n";
    return fails != 0;
}
REGISTER_HCCL_TEST(stress, test_stress);

// --------------------------- CLI & main ------------------------------------
static void print_usage(const char *prog)
{
    std::cout << "Usage: " << prog
              << " --cluster_info <path_or_json> --rank N [--peers P] [--size B] [--test name] [--list]\n";
}

int main(int argc, char **argv)
{
    Config cfg;
    std::string selectedTest;

    static struct option long_options[] = {{"cluster_info", required_argument, 0, 'c'},
        {"rank", required_argument, 0, 'r'},
        {"peers", required_argument, 0, 'p'},
        {"size", required_argument, 0, 's'},
        {"test", required_argument, 0, 't'},
        {"engine", required_argument, 0, 'e'},
        {"list", no_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};
    int opt;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "c:r:p:s:t:e:lh", long_options, &long_index)) != -1) {
        switch (opt) {
            case 'c':
                cfg.clusterInfo = optarg;
                break;
            case 'r':
                cfg.rank = static_cast<uint32_t>(atoi(optarg));
                break;
            case 'p':
                cfg.peers = static_cast<uint32_t>(atoi(optarg));
                break;
            case 's':
                cfg.msgSize = static_cast<uint64_t>(atoll(optarg));
                break;
            case 't':
                selectedTest = optarg;
                break;
            case 'e':
                cfg.commEngine = static_cast<CommEngine>(atoll(optarg));
                break;
            case 'l':
                ListAllTests();
                return 0;
            case 'h':
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // fallback to env vars if not provided
    if (cfg.clusterInfo.empty()) {
        const char *env_rt = getenv("RANK_TABLE_FILE");
        if (env_rt)
            cfg.clusterInfo = env_rt;
    }
    if (cfg.clusterInfo.empty()) {
        std::cerr << "cluster_info required (path or JSON). Set --cluster_info or RANK_TABLE_FILE env var\n";
        return 1;
    }
    const char *env_rank = getenv("RANK_ID");
    if (env_rank)
        cfg.rank = static_cast<uint32_t>(atoi(env_rank));

    printf("[MAIN] Config: clusterInfo[%s], rank[%u], peers[%u], test[%s], iters[%u], msgSize[%llu], commEngine[%d] "
        "threadNum[%u]\n", cfg.clusterInfo.c_str(), cfg.rank, cfg.peers, cfg.test.c_str(), cfg.iters,
        cfg.msgSize, cfg.commEngine, cfg.threadNum);

    // If user specified a single test name, run only that
    if (!selectedTest.empty()) {
        auto it = GetTestRegistry().find(selectedTest);
        if (it == GetTestRegistry().end()) {
            std::cerr << "Unknown test: " << selectedTest << "\n";
            ListAllTests();
            return 2;
        }
        bool ok = false;
        try {
            ok = it->second(cfg);
        } catch (const std::exception &e) {
            std::cerr << "[EXCEPTION] " << e.what() << "\n";
            ok = false;
        }
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << selectedTest << std::endl;
        return ok ? 0 : 3;
    }

    // otherwise run all registered tests in order
    int fail_count = 0;
    for (auto &kv : GetTestRegistry()) {
        const std::string &name = kv.first;
        std::cout << ">>> Running test: " << name << " <<<\n";
        bool ok = false;
        try {
            ok = kv.second(cfg);
        } catch (const std::exception &e) {
            std::cerr << "[EXCEPTION] " << e.what() << "\n";
            ok = false;
        }
        if (!ok) {
            std::cerr << "[RESULT] test failed: " << name << "\n";
            ++fail_count;
        } else {
            std::cout << "[RESULT] test passed: " << name << "\n";
        }
    }

    std::cout << "All tests finished. " << (fail_count ? "FAILED" : "PASSED") << "\n";
    return fail_count ? 4 : 0;
}