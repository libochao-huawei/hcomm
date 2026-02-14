#include <dlog_pub.h>
#include <stdio.h>

int32_t dlog_setlevel(int32_t moduleId, int32_t level, int32_t enableEvent)
{
    return 0;
}

int32_t CheckLogLevel(int32_t moduleId, int32_t logLevel)
{
    // return 1;
    return logLevel >= 3 ? 1 : 0;
}

int32_t DlogSetAttr(LogAttr logAttrInfo)
{
    return 0;
}

void DlogVaList(int32_t moduleId, int32_t level, const char *fmt, va_list list)
{
    vprintf(fmt, list);
    printf("\n");
}

void DlogRecord(int32_t moduleId, int32_t level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DlogVaList(moduleId, level, fmt, args);
    va_end(args);
}