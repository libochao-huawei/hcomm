
#include "mirror_task_manager.h"
#include "hcclCommProfilingLite.h"

typedef  struct {
    int id;
} HcclOpInfo;

namespace hccl {
class HcclCommDfxLite {
public:
    // 构造函数（接收CommunicatorImplLite中已经存在的MirrorTaskManager指针）
    explicit HcclCommDfxLite(Hccl::MirrorTaskManager* existingMirrorTaskManager = nullptr);
    
    // 初始化DFX系统
    void Init();
    
    // 注册回调到单例
    void RegisterProfilingCallback();
    
    // 获取MirrorTaskManager
    Hccl::MirrorTaskManager* GetMirrorTaskManager() const;
    
    // Profiling相关接口（直接暴露，不通过GetProfilingImpl）
    void ReportAllTasks();
    void ReportHcclOpInfo(const HcclOpInfo& hcclOpInfo);
    void UpdateProfStat();

private:
    Hccl::MirrorTaskManager* mirrorTaskManager_;  // 使用原始指针，不管理生命周期
    std::unique_ptr<HcclCommProfilingLite> profilingImpl_;
};

}