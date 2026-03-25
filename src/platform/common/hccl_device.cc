#include "hccl_device.h"
#include <unistd.h>
#include <sys/syscall.h>
#include <mutex>
#include <string>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
 
std::mutex g_logLock;
 
inline const uint64_t GetLogPid()
{
    static const uint64_t pid = getpid();
    return pid;
}
 
inline const uint64_t GetLogTid()
{
    thread_local static const uint64_t tid = static_cast<uint64_t>(syscall(__NR_gettid));
    return tid;
}
 
std::string g_fileName = "/var/log/npu/slog/" + std::to_string(GetLogPid()) + "_hccl.log";
 
void logC(const char *func, const char *file, const int line,
          const char *type, const char *format, ...)
{
    FILE *file_fp;
    time_t loacl_time;
    char time_str[128];
 
    // 获取本地时间
    time(&loacl_time);
    strftime(time_str, sizeof(time_str), "[%Y.%m.%d %X]", localtime(&loacl_time));
 
    // 日志内容格式转换
    va_list ap;
    va_start(ap, format);
    char fmt_str[2048];
    vsnprintf(fmt_str, sizeof(fmt_str), format, ap);
    va_end(ap);
 
    // 写日志文件
    std::lock_guard<std::mutex> lk(g_logLock);
    file_fp = fopen(g_fileName.c_str(), "a");
    if (file_fp != NULL) {
        fprintf(file_fp, "%s%s[%s@%s:%d][tid:%lu] %s\n", type, time_str, func, 
                file, line, GetLogTid(), fmt_str);
        fclose(file_fp);
    }
}