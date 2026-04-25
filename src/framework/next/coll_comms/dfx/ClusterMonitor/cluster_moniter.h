#ifndef CLUSTER_NOBITOR_H
#define CLUSTER_NOBITOR_H

#include <string>
#include <deque>
#include <vector>
#include <mutex>
#include <queue>


#include "reference_map.h"
#include "hccl_types.h"
#include "log.h"
#include "sal_pub.h"
#include "topoinfo_struct.h"
#include "ip_address.h"


namespace hcomm {
constexpr u32 EVENT_MAX_CNT = 5000;          // 防止内存泄漏，同时不能太短，防止正常事件被冲掉
constexpr u32 OPINFO_SEND_NUM_BY_TAG = 500;   // 一次心跳帧发送的算子信息个数
constexpr u32 OPINFO_TAG_QUEUE_NUM = 10;   // 一次心跳帧发送的算子信息个数

typedef unsigned int u32;
using UIDType = struct HcclClusterMonitorUid {
    char id[512] = {0}; // ip[IP_ADDRESS_BUFFER_LEN] + ifname[MAX_INTERFACE_NAME_LEN] + devid 最大不超过512字节
    bool operator == (const HcclClusterMonitorUid &that) const
    {
        return std::string(this->id) == std::string(that.id);
    }
    bool operator != (const HcclClusterMonitorUid &that) const
    {
        return std::string(this->id) != std::string(that.id);
    }
    bool operator < (const HcclClusterMonitorUid &that) const
    {
        return std::string(this->id) < std::string(that.id);
    }
};

}

namespace std {
template <> class hash<hcomm::HcclClusterMonitorUid> {
public:
    size_t operator () (const hcomm::HcclClusterMonitorUid &uid) const
    {
        return hash<string>()(string(uid.id));
    }
};
}

namespace hcomm {
struct ConnInfo {
    u32 lostNum = 0;
    bool newConn = false;
    ConnInfo() {}
};
enum class HeartBeatStatus {
    HEARTBEAT_OK,
    HEARTBEAT_LOST,
    HEARTBEAT_EXCEPTION,
    HEARTBEAT_CQE_EXCEPTION
};


struct HeartBeatFrame {
    UIDType src;
    UIDType dst;
    UIDType crimer;
    UIDType informer;
    HeartBeatStatus status = HeartBeatStatus::HEARTBEAT_OK;
    HcclUs TOARelative; // time of arrival (Relative)
    HcclSystemTime TOASystem; // time of arrival (System)
    HeartBeatFrame() {}
    HeartBeatFrame(UIDType &crimer, UIDType &informer, HeartBeatStatus status, HcclUs TOARelativeIn,
        HcclSystemTime TOASystemIn)
        : crimer(crimer), informer(informer), status(status), TOARelative(TOARelativeIn),
        TOASystem(TOASystemIn)
    {}
    HeartBeatFrame(UIDType &src, UIDType &dst, UIDType &crimer, UIDType &informer, HeartBeatStatus status)
        : src(src), dst(dst), crimer(crimer), informer(informer), status(status)
    {}
};

const std::map<HeartBeatStatus, std::string> HEARTBEAT_STATUS_STR_MAP{
    {HeartBeatStatus::HEARTBEAT_OK, "OK"},
    {HeartBeatStatus::HEARTBEAT_LOST, "LOST"},
};

inline std::string GetHeartBeatStatusStr(HeartBeatStatus  status)
{
    auto iter = HEARTBEAT_STATUS_STR_MAP.find(status);
    if (iter == HEARTBEAT_STATUS_STR_MAP.end()) {
        return "Unknown";
    } else {
        return iter->second;
    }
}

struct CqeErrInfo{
    u32 CqeRemoterankId;
    uint16_t CqeRemoterstatus;
    std::string CqeLocalEid; 
    std::string CqeRemoteEid;
};

class ClusterMonitor {
public:
    static ClusterMonitor& GetInstance();
    void GetRemoteRankId(u32 rankId, uint16_t status, std::string LocalEid, std::string RemoteEid);
    ClusterMonitor() = default;
    ~ClusterMonitor() = default;
private:
    void RecvFrameOutCheck();
    HcclResult RecvFrame(UIDType &src);
    HcclResult SendFrame(UIDType &dst, UIDType &crimer, UIDType &informer, HeartBeatStatus status);
    void SetStatus(UIDType &crimer, UIDType &informer, HeartBeatStatus status, bool needBroadcast = true);
    std::string FormatUId(const UIDType& uid) const;
    void ProcessExceptionEvent();
    struct Status
    {
        HeartBeatStatus status = HeartBeatStatus::HEARTBEAT_OK;
        UIDType informer;
        bool needBroadcast = false;
        Status() {}
    };
    u32 lostThreshold_ = 0;
    ReferenceMap<UIDType, ConnInfo> rankId2SocketMap_;
    ReferenceMap<UIDType, Status> rankId2StatusMap_;
    std::vector<UIDType> errorSocket_;
    UIDType uid_;
    std::queue<UIDType> errRankQueue_;
    std::queue<HeartBeatFrame> errStatusQueue_;
    CqeErrInfo CqeErrInfo_;
};
}

#endif