#include "atrace_api.h"
/**
 * @brief       Create trace handle.
 * @param [in]  tracerType:    trace type
 * @param [in]  objName:       object name
 * @param [in]  attr:          object attribute
 * @return      atrace handle
 */
TraHandle AtraceCreateWithAttr(TracerType tracerType, const char *objName, const TraceAttr *attr)
{
    return 0;
}

/**
 * @brief       Submite trace info
 * @param [in]  handle:    trace handle
 * @param [in]  buffer:    trace info buffer
 * @param [in]  bufSize:   size of buffer
 * @return      TraStatus
 */
TraStatus AtraceSubmit(TraHandle handle, const void *buffer, uint32_t bufSize)
{
    if (handle == 0) {
    return 0;
    } else if (handle == 1) {
        return -1;
    }
    return 0;
}

/**
 * @brief       Destroy trace handle
 * @param [in]  handle:    trace handle
 * @return      NA
 */
void AtraceDestroy(TraHandle handle)
{
    return;
}