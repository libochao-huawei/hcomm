#include "cluster_moniter.h"
#include "hccl_common.h"
//#include "../taskException/host/hcclCommTaskException.h"
#include "hcclCommTaskException.h"
#include "ccuTaskException.h"

#include "../../env_config/env_config.h"

namespace hcomm {
constexpr u32 HEARTBEAT_INTERVAL = 1000; 
constexpr u32 JITTER_TIME = 300; // 关键事件允许的误差事件范围±300s。误差来源：EVENT和NOTIFY差异、传播耗时、计时误差   
constexpr u32 MAX_MODULE_DEVICE_NUM = 65;
HcclResult ret;
ClusterMonitor &ClusterMonitor::GetInstance(u32 deviceId)
{
    static ClusterMonitor hb[MAX_MODULE_DEVICE_NUM];
    if (static_cast<u32>(deviceId) >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[Heartbeat][%s]deviceId[%d] is invalid", __func__, deviceId);
        return hb[0];
    }
    return hb[deviceId];
}

std::string ClusterMonitor::FormatUId(const UIDType &uid) const
{
    return uid.id;
}
void ClusterMonitor::SetStatus(UIDType &crimer, UIDType &informer, HeartBeatStatus status, bool needBroadcast)
{
    if (rankId2StatusMap_[crimer].status != status) {
        rankId2StatusMap_[crimer].informer = informer;
        rankId2StatusMap_[crimer].status = status;
        rankId2StatusMap_[crimer].needBroadcast = needBroadcast;
        if (needBroadcast) {
            errRankQueue_.push(crimer);
        }

        errStatusQueue_.push(HeartBeatFrame(crimer, informer, status, TIME_NOW(), std::chrono::system_clock::now()));

        if (errStatusQueue_.size() > EVENT_MAX_CNT) {
            errStatusQueue_.pop();
        }
        HCCL_RUN_INFO("[%s][%s]local rank [%s]: crimer rank [%s] status[%s] by informer rank [%s]",
            LOG_KEYWORDS_TASK_EXEC.c_str(), LOG_KEYWORDS_HEARTBEAT_EVETN.c_str(), FormatUId(uid_).c_str(),
            FormatUId(crimer).c_str(), GetHeartBeatStatusStr(status).c_str(), FormatUId(informer).c_str());
    }
}

void ClusterMonitor::RecvFrameOutCheck()
{
    for (auto iter = rankId2SocketMap_.begin(); iter != rankId2SocketMap_.end(); iter++) {
            UIDType rem = iter->first;
            HCCL_DEBUG("rank[%s] Try to Recv from rank[%s]", FormatUId(uid_).c_str(), FormatUId(rem).c_str());
            ret =  RecvFrame(rem);
            if (ret == HCCL_E_INTERNAL) {
                errorSocket_.push_back(rem);
            } else if (rankId2SocketMap_[rem].lostNum >= lostThreshold_) {
                SetStatus(rem, uid_, HeartBeatStatus::HEARTBEAT_LOST);
            }
        }
    return;
}

HcclResult ClusterMonitor::FormatUID(std::string instanceId, u32 localId, UIDType &uid)
{
    s32 ret = snprintf_s(uid.id, sizeof(uid.id), sizeof(uid.id) - 1, "%s%s%s",
 	         instanceId.c_str(), "/", std::to_string(localId).c_str());
 	     CHK_PRT_RET((ret == -1),
 	         HCCL_ERROR("[%s] snprintf_s failed, errno:%d, error:%s",
 	             __func__, errno, strerror(errno)), HCCL_E_SYSCALL);
    return HCCL_SUCCESS;
}


void ClusterMonitor::ProcessExceptionEvent()
{
     while (errRankQueue_.size() > 0) {
        UIDType cur = errRankQueue_.front();
        rankId2StatusMap_[cur].needBroadcast = false;
        for (auto iterRem = rankId2SocketMap_.begin(); iterRem != rankId2SocketMap_.end(); iterRem++) {
            UIDType rem = iterRem->first;
            if (rem != rankId2StatusMap_[cur].informer &&
                rankId2StatusMap_[rem].status == HeartBeatStatus::HEARTBEAT_OK) {
                (void)SendFrame(rem, cur, rankId2StatusMap_[cur].informer, rankId2StatusMap_[cur].status);        
            }
        }
        errRankQueue_.pop();
    }
    return;
}

bool ClusterMonitor::IsKeyEvent(HeartBeatFrame &event, HcclUs curTime)
{
    bool ret = false;
    s64 intervalTime = DURATION_US(curTime - event.TOARelative).count() / (TIME_S_TO_MS * ONE_MILLISECOND_OF_USLEEP);
    const s32 hcclExecTimeout = Hccl::EnvConfig::GetInstance().GetRtsConfig().GetExecTimeOut();
    s64 execTimeout = hcclExecTimeout;
    s64 detectionTime = 0;
    switch (event.status) {
        case HeartBeatStatus::HEARTBEAT_LOST:
            detectionTime = (lostThreshold_ * HEARTBEAT_INTERVAL) / TIME_S_TO_MS;
            break;
        case HeartBeatStatus::HEARTBEAT_CQE_ERR:
            detectionTime = 0;
            break;
        default:
            return false; // 当前不支持的事件，不做处理和展现
    }
    ret = ((execTimeout - intervalTime - detectionTime) < JITTER_TIME) &&
        ((intervalTime + detectionTime - execTimeout) < JITTER_TIME);
    return ret;
}

void ClusterMonitor::MakeErrMsg(std::queue<HeartBeatFrame> &keyEvents, std::vector<std::string> &errStatusVec)
{
    while (keyEvents.size() > 0) {
        auto &tmp = keyEvents.front();
        std::string crimerStr = FormatUId(tmp.crimer);
        std::string informerStr = FormatUId(tmp.informer);

        std::string headStr = "[" + LOG_KEYWORDS_TASK_EXEC + "][" + LOG_KEYWORDS_HEARTBEAT_EVETN + "]" +
            "Cluster Exception Location[IP/ID]:[";

        time_t tm = std::chrono::system_clock::to_time_t(tmp.TOASystem);
        std::string timeStr(ctime(&tm));
        if (!timeStr.empty()) { // ctime()函数自带换行符，需要去掉
            timeStr.pop_back();
        }
        timeStr = ", Arrival Time:[" + timeStr + "]";

        std::string errStr = ", ExceptionType:";
        std::string reasonStr = ", Possible Reason:";
        switch (tmp.status) {
            case HeartBeatStatus::HEARTBEAT_LOST:
                errStr = errStr + "[Heartbeat Lost Occurred]";
                reasonStr = reasonStr + "1. Process has exited, 2. Network Disconnected";
                errStr =
                    headStr + crimerStr + "]" + timeStr + ", Discoverer:[" + informerStr + "]" + errStr + reasonStr;
                break;
            case HeartBeatStatus::HEARTBEAT_CQE_ERR:
                errStr = errStr + "[Error cqe Occurred]";
                reasonStr = reasonStr + "1.Network Disconnected, 2.Remote Rank Coredown";
                errStr = headStr + crimerStr + "]" + timeStr + errStr + reasonStr;
                break;
            default:
                errStr = " Unknown";
        }
        errStatusVec.emplace_back(errStr);
        keyEvents.pop();
    }
}

std::vector<std::string> ClusterMonitor::PrintEvents(std::map<HeartBeatStatus, std::queue<HeartBeatFrame>> &keyEvents)
{
    std::vector<std::string> errStatusVec;
    // 打印优先级 opretry not support > error cqe > stuck > lost
    MakeErrMsg(keyEvents[HeartBeatStatus::HEARTBEAT_CQE_ERR], errStatusVec);
    MakeErrMsg(keyEvents[HeartBeatStatus::HEARTBEAT_LOST], errStatusVec);
    return errStatusVec;
}

std::vector<std::string> ClusterMonitor::GetErrStatusVec()
{
    std::unique_lock<std::mutex> lock(ProcessLock_);
    HcclUs curTime = TIME_NOW();
    std::map<HeartBeatStatus, std::queue<HeartBeatFrame>> keyEvents;
    while (errStatusQueue_.size() > 0) {
        auto &tmp = errStatusQueue_.front();
        if (IsKeyEvent(tmp, curTime)) { // 非关键事件不处理
            keyEvents[tmp.status].push(tmp);
        }
        errStatusQueue_.pop();
    }
    return PrintEvents(keyEvents);
}

std::vector<std::string> GetErrStatusVec(s32 deviceLogicID)
{
    return ClusterMonitor::GetInstance(deviceLogicID).GetErrStatusVec();
}

void GetCqeErrInfo(unsigned int RemoteLocalIdId, unsigned int LocDeviceId, unsigned short int status, std::string LocalEid, std::string RemoteEid, std::string RemoteInsId)
{
    return ClusterMonitor::GetInstance(LocDeviceId).GetCqeErrInfo(RemoteLocalIdId, status, LocalEid, RemoteEid, RemoteInsId);
}

void ClusterMonitor::GetCqeErrInfo(u32 RemoteLocalIdId, uint16_t status, std::string LocalEid, std::string RemoteEid, std::string RemoteInsId)
{
    CqeErrInfo_.CqeRemoteLocalId = RemoteLocalIdId;
    CqeErrInfo_.Cqestatus = status;
    CqeErrInfo_.CqeLocalEid = LocalEid;
    CqeErrInfo_.CqeRemoteEid = RemoteEid;
    CqeErrInfo_.CqeRemoteInsId = RemoteInsId;
    UIDType remoteUid = {0};
    UIDType localUid = myRankUid_;
    CHK_RET_NULL(FormatUID(CqeErrInfo_.CqeRemoteInsId, CqeErrInfo_.CqeRemoteLocalId, remoteUid));
    SetStatus(localUid, remoteUid, HeartBeatStatus::HEARTBEAT_CQE_ERR);

    // if (CqeErrInfo_.CqeRemoterstatus != 0) {
    //    SetStatus(uid_, uid_, HeartBeatStatus::HEARTBEAT_CQE_EXCEPTION);//从远端获取的remoteRankId需要转换为Uid_类型
    //    HCCL_RUN_INFO("[%s][%s]local rank [%s]: crimer rank [%s] status[%d] by informer rank [%d]",
    //         LOG_KEYWORDS_TASK_EXEC.c_str(), LOG_KEYWORDS_HEARTBEAT_EVETN.c_str(), FormatUId(uid_).c_str(),
    //         FormatUId(uid_).c_str(), CqeErrInfo_.CqeRemoterstatus, CqeErrInfo_.CqeRemoterankId);
    // }
    return;
}

__attribute__((constructor)) void ClusterMonitorCallBackInit()
{
    hcomm::RegisterGetAicpuCqeErrInfoCallBackHcomm(GetCqeErrInfo);
    hcomm::RegisterGetCcuCqeErrInfoCallBackHcomm(GetCqeErrInfo);
    hcomm::RegisterAicpuGetErrStatusVecCallBack(GetErrStatusVec);
    hcomm::RegisterCcuGetErrStatusVecCallBack(GetErrStatusVec);
}


}