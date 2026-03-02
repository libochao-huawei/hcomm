#ifndef __COLL_COMM_AICPU_H__
#define __COLL_COMM_AICPU_H__

#include "common.h"
#include "aicpu_init_param.h"
#include "topo_matcher.h"
#include "hcomm_primitives.h"
#include "transport_pub.h"
#include "thread.h"
#include "local_notify.h"
#include "ub_transport_lite_impl.h"
#include "task_exception.h"
#include "aicpu_launch_manager.h"
#include "channel_param.h"
#include "hdc_pub.h"

namespace hccl {
class CollCommAicpu {
public:
    // N秒快恢
    void NsCommClean();
    KfcCommand BackGroundGetCmd(){}
    void BackGroundSetStatus(KfcStatus status, KfcErrType errorCode = KfcErrType::NONE){}
    void SetIsCommReady(bool flag)
    {
        isCommReady = flag;
    }
    bool IsCommReady() const
    {
        return isCommReady;
    }
    void SetNeedClean(bool flag)
    {
        needClean = flag;
    }
    bool IsNeedClean() const
    {
        return needClean;
    }
    void SetIsSuspended(bool status)
    {
        isSuspended = status;
    }
    bool IsSuspended() const
    {
        return isSuspended;
    }
    std::vector<std::shared_ptr<Thread>> GetThreads();

    HcclResult SetDispatcherCtxOnThread();
private:
// 独立算子
    bool indOpCommInitialized_{ false }; // 独立算子流程通信域是否初始化
    DispatcherCtxPtr dispatcherCtx_{nullptr};
    std::unordered_map<std::string, ChannelHandle> channelHandleMap_;
    std::unordered_map<ChannelHandle, std::shared_ptr<hccl::Transport>> linkMap_;
    std::vector<std::shared_ptr<hccl::Thread>> threads_;
    std::vector<std::unique_ptr<LocalNotify>> notifys_;
    TaskException taskExecption_;
    // A5 独立算子
    std::unordered_map<ChannelHandle, std::unique_ptr<Hccl::UbTransportLiteImpl>> ubTransportMap_;

    bool isCommReady;
    bool needClean;
    bool isSuspended;
};

}

#endif