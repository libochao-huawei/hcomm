#include <stdint.h>

typedef enum {
    TSD_HCCP = 0,    /**< HCCP*/
    TSD_COMPUTE, /**< Compute_process*/
    TSD_CUSTOM_COMPUTE, /**< Custom Compute_process*/
    TSD_QS,
    TSD_WAITTYPE_MAX /**< Max*/
} TsdWaitType;

int SendStartUpFinishMsg(int dev_id, int type, unsigned int host_pid, unsigned int vfId);
int32_t ReportProcessStartUpErrorCode(const uint32_t deviceId, const TsdWaitType waitType,
    const uint32_t hostPid, const uint32_t vfId,
    const char *errCode, const uint32_t errLen);