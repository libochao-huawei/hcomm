#include "slog.h"
#include "slog_api.h"
#include <stdint.h>
#include <syslog.h>
#include <stdio.h>
int g_UT_LOG_LEVEL = DLOG_DEBUG;

int CheckLogLevel(int moduleId, int logLevel)
{
    (void)moduleId;
    (void)logLevel;
    return 1;
}

void DlogRecord(int moduleId, int level, const char *fmt, ...)
{
    (void)moduleId;
    (void)fmt;
    if (level >= g_UT_LOG_LEVEL) {
        va_list list;
        va_start(list, fmt);
        vprintf(fmt, list);
        va_end(list);
    }
}

void DlogInner(int moduleId, int level, const char *fmt, ...)
{
    (void)moduleId;
    (void)fmt;
    if (level >= g_UT_LOG_LEVEL) {
        va_list list;
        va_start(list, fmt);
        vprintf(fmt, list);
        va_end(list);
    }
}