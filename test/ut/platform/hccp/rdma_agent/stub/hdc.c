#include "ascend_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DLLEXPORT hdcError_t drvHdcInit(enum drvHdcServiceType serviceType)
{
    return DRV_ERROR_NONE;
}

DLLEXPORT hdcError_t drvHdcUninit(void)
{
}

DLLEXPORT hdcError_t drvHdcSendFile(int peer_node, int peer_devid, const char *file, const char *dst_path,
                                    void (*progress_notifier)(struct drvHdcProgInfo *))
{
    return DRV_ERROR_NONE;
};

