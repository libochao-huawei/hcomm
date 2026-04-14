#ifndef ASCEND_HAL_STUB_H
#define ASCEND_HAL_STUB_H

#include <sys/syscall.h>

#define SHM_KEY 0x1234ABCD
#define DRV_LOG_INFO(fmt, ...)
// #define DRV_LOG_INFO(fmt, ...) printf("[INFO][" LOG_SIDE "] " fmt "\n", ##__VA_ARGS__)
#define DRV_LOG_ERROR(fmt, ...) printf("[ERROR][" LOG_SIDE "][%s:%d]pid:%d,tid:%ld,%s :" fmt "\n", __FILE__, __LINE__, getpid(), syscall(SYS_gettid), __func__, ##__VA_ARGS__)

#ifndef container_of
#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type, member) );})
#endif

#define LISTEN_BACKLOG      5
#define MAX_DEV_ID          64

typedef struct {
    int sessionId;
    int conn_fd;
    unsigned int hostPhyId;
    unsigned int devPhyId;
    int lastRecvStatus;
    int isUsed;
    int peerNode;
    pid_t hostPid;
    pid_t devPid;
    pthread_mutex_t lock;
} HdcSessionT;

typedef struct ServerInfo {
    int listen_fd;
    unsigned int phyId;
    int port;
} ServerInfoT;

struct hdc_msg_head {
    bool freeBuf;
    struct drvHdcMsg msg;
};

static int GetPortByServiceType(int phyId)
{
    return 9999 + phyId; 
}

static int GetPeerPortByDevid(int peerDevid)
{
    return 9999 + peerDevid;
}

typedef struct h2d_info {
    pid_t hostPid;
    pid_t devPid;
    unsigned int hostLogicId;
    unsigned int devLogicId;
    unsigned int hostPhyId;
    unsigned int devPhyId;
} h2d_info_t;

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

void setHostPid(pid_t pid, int phyId);
void setDevPid(pid_t pid, int phyId);
void InitHdcId();
h2d_info_t *GetH2dInfo();
int AllocSessionId(void);
HdcSessionT **GetSession();
#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // ASCEND_HAL_STUB_H