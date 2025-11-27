#include <stdint.h>
#include "tsd.h"

int SendStartUpFinishMsg(int dev_id, int type, unsigned int host_pid, unsigned int vfId)
{
	return 0;
}

int32_t ReportProcessStartUpErrorCode(const uint32_t deviceId, const TsdWaitType waitType,
    const uint32_t hostPid, const uint32_t vfId,
    const char *errCode, const uint32_t errLen)
{
        return 0;
}