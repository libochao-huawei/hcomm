#include "cluster_moniter.h"
#include "hccl_common.h"


namespace hccl {
HcclResult ret;

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
void ClusterMonitor::GetRemoteRankId(u32 rankId, uint16_t status)
{
    u32 remoteRankId = rankId;
    uint16_t remoteStatus = status;
    if (remoteStatus != 0) {
       SetStatus(uid_, remoteRankId, HeartBeatStatus::HEARTBEAT_CQE_EXCEPTION);//从远端获取的remoteRankId需要转换为Uid_类型
       HCCL_RUN_INFO("[%s][%s]local rank [%s]: crimer rank [%s] status[%s] by informer rank [%s]",
            LOG_KEYWORDS_TASK_EXEC.c_str(), LOG_KEYWORDS_HEARTBEAT_EVETN.c_str(), FormatUId(uid_).c_str(),
            FormatUId(uid_).c_str(), GetHeartBeatStatusStr(status).c_str(), FormatUId(remoteRankId).c_str());
    }
    return;
}
}