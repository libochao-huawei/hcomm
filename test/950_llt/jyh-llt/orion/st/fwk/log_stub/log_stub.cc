#include "slog.h"
#include "slog_api.h"
#include "atrace_api.h"
int CheckLogLevel(int moduleId, int logLevel)
{
    return 0;
}

void DlogRecord(int moduleId, int level, const char *fmt, ...)
{}

void DlogInner(int moduleId, int level, const char *fmt, ...)
{}

TraHandle AtraceCreateWithAttr(TracerType tracerType, const char *objName, const TraceAttr *attr)
{
    return 0;
}

TraStatus AtraceSubmit(TraHandle handle, const void *buffer, uint32_t bufSize)
{
    return 0;
}

void AtraceDestroy(TraHandle handle)
{
    return;
}