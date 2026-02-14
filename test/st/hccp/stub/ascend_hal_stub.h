#ifndef ASCEND_HAL_STUB_H
#define ASCEND_HAL_STUB_H

#define LISTEN_BACKLOG      5
#define MAX_DEV_ID 64
typedef struct {
    int sessionId;          // 驱动会话ID（唯一）
    int conn_fd;          // TCP连接文件描述符（核心：Client连接fd）
    unsigned int hostPhyId;     // 所属物理通道
    unsigned int devPhyId;      // 设备物理通道
    int lastRecvStatus;     // 驱动状态
    int isUsed;             // 会话是否启用
    int peerNode;
    pid_t hostPid;      // Host PID（从共享内存获取）
    pid_t devPid;       // Device PID（从共享内存获取）
    pthread_mutex_t lock;
} HdcSessionT;


typedef struct ServerInfo {
    int listen_fd;
    unsigned int phyId;
    int port;    // 监听端口（用于调试）
} ServerInfoT;

/************************* 3. 全局管理器 *************************/
static HdcSessionT *g_hdcMgr[MAX_DEV_ID] = {0};
static int g_sessionIdSeed = 1000;

// ==================== 工具函数：原子分配唯一会话ID ====================
static int AllocSessionId(void)
{
    return __sync_fetch_and_add(&g_sessionIdSeed, 1);
}

// ==================== 根据服务类型映射监听端口（可自定义） ====================
static int GetPortByServiceType(int serviceType)
{
    return 9999; 
}

static int GetPeerPortByDevid(int peerDevid)
{

    return 9999;
}

// ==================== 配置 ====================
// 指向共享内存的指针（所有进程访问同一块物理内存）
typedef struct h2d_info {
    pid_t hostPid;
    pid_t devPid;
    unsigned int hostLogicId;
    unsigned int devLogicId;
    unsigned int hostPhyId;
    unsigned int devPhyId;
} h2d_info_t;
static h2d_info_t *g_h2dInfo = NULL;
// 共享内存键值（自定义固定值，保证所有进程识别同一块内存）
#define SHM_KEY 0x1234ABCD

#define SHM_SIZE (sizeof(h2d_info_t) * MAX_DEV_ID)

// 全局共享内存ID（进程内私有，不影响共享数据）
static int g_shmid = -1;

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus


void setHostPid(pid_t pid);
void setDevPid(pid_t pid);
void InitHdcId();

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // ASCEND_HAL_STUB_H