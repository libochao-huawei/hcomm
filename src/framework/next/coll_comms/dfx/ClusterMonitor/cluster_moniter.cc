#include "cluster_moniter.h"
#include "hccl_common.h"
#include "../taskException/host/hcclCommTaskException.h"

namespace hcomm {
HcclResult ret;
ClusterMonitor &ClusterMonitor::GetInstance()
{
    static ClusterMonitor hb;
    // if (static_cast<u32>(deviceLogicID) >= MAX_MODULE_DEVICE_NUM) {
    //     HCCL_WARNING("[Heartbeat][%s]deviceLogicID[%d] is invalid", __func__, deviceLogicID);
    //     return hb[0];
    // }
    return hb;
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
void GetRemoteRankId(unsigned int rankId, unsigned short int status, std::string LocalEid, std::string RemoteEid)
{
    return ClusterMonitor::GetInstance().GetRemoteRankId(rankId, status, LocalEid, RemoteEid);
}

void ClusterMonitor::GetRemoteRankId(u32 rankId, uint16_t status, std::string LocalEid, std::string RemoteEid)
{
    CqeErrInfo_.CqeRemoterankId = rankId;
    CqeErrInfo_.CqeRemoterstatus = status;
    CqeErrInfo_.CqeLocalEid = LocalEid;
    CqeErrInfo_.CqeRemoteEid = RemoteEid;
    if (CqeErrInfo_.CqeRemoterstatus != 0) {
       SetStatus(uid_, uid_, HeartBeatStatus::HEARTBEAT_CQE_EXCEPTION);//从远端获取的remoteRankId需要转换为Uid_类型
       HCCL_RUN_INFO("[%s][%s]local rank [%s]: crimer rank [%s] status[%d] by informer rank [%d]",
            LOG_KEYWORDS_TASK_EXEC.c_str(), LOG_KEYWORDS_HEARTBEAT_EVETN.c_str(), FormatUId(uid_).c_str(),
            FormatUId(uid_).c_str(), CqeErrInfo_.CqeRemoterstatus, CqeErrInfo_.CqeRemoterankId);
    }
    return;
}

__attribute__((constructor)) void ClusterMonitorCallBackInit()
{
    hcomm::RegisterGetAicpuRemoteRankIdCallBackHcomm(GetRemoteRankId);
}



}