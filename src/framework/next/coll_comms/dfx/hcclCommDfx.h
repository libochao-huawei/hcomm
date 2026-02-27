#ifndef HCCL_COMM_DFX_H
#define HCCL_COMM_DFX_H

#include <memory>
#include "mirror_task_manager.h"
#include "hcclCommProfiling.h"
#include "global_mirror_tasks.h"

namespace hccl {

class HcclCommDfx {
public:
    // 构造函数（接收CommunicatorImpl中已经存在的MirrorTaskManager指针）
    explicit HcclCommDfx(uint32_t deviceId);
    
    // 初始化DFX系统
    void Init();
    
    // 注册回调到单例
    void RegisterProfilingCallback();
    
    // 获取MirrorTaskManager
    MirrorTaskManager* GetMirrorTaskManager() const;
    
    // Profiling相关接口（直接暴露，不通过GetProfilingImpl）
    void ReportAllTasks(bool cachedReq = false);
    void ReportOp(uint64_t beginTime, bool cachedReq, bool opbased);
    // void CallReportMc2CommInfo(const Mc2CommInfo& mc2CommInfo);
    void UpdateProfStat();
    
private:
    uint32_t deviceId_;
    MirrorTaskManager* mirrorTaskManager_;  // 使用原始指针，不管理生命周期
    std::unique_ptr<HcclCommProfiling> profiling_;
};

}

#endif
