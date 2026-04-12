#include <dlog_pub.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>
#include <linux/limits.h>
#include <libgen.h>
#include <errno.h>
#include <libgen.h>

static FILE* g_log_file = NULL;
static int g_global_log_level = 3;
static char g_log_path[PATH_MAX] = {0};
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    char *p = NULL;
    struct stat st;

    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (stat(tmp, &st) != 0) {
                if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                    return -1;
                }
            } else if (!S_ISDIR(st.st_mode)) {
                errno = ENOTDIR;
                return -1;
            }
            *p = '/';
        }
    }

    if (stat(tmp, &st) != 0) {
        if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
            return -1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    return 0;
}

static void __attribute__((constructor)) dlog_stub_init(void)
{
    char* env_level = getenv("ASCEND_GLOBAL_LOG_LEVEL");
    if (env_level != NULL) {
        g_global_log_level = atoi(env_level);
    }
    
    char exe_path[PATH_MAX] = {0};
    ssize_t count = readlink("/proc/self/exe", exe_path, PATH_MAX);
    if (count == -1) {
        perror("readlink failed to get exe path");
        g_log_file = stdout;
        return;
    }
    exe_path[count] = '\0';
    
    char* dir_path = dirname(exe_path);
    char log_dir[PATH_MAX] = {0};
    
    int ret = snprintf(log_dir, sizeof(log_dir), "%s/../ascend/log", dir_path);
    if (ret < 0) {
        perror("snprintf failed for log_dir");
        g_log_file = stdout;
        return;
    } else if ((size_t)ret >= sizeof(log_dir)) {
        fprintf(stderr, "Error: Log directory path truncated\n");
        g_log_file = stdout;
        return;
    }
    
    if (mkdir_p(log_dir, 0755) == -1) {
        if (errno != EEXIST) {
            perror("mkdir failed (will try to continue)");
        }
    }
    
    pid_t pid = getpid();
    
    ret = snprintf(g_log_path, sizeof(g_log_path), "%s/hccp_test-%d.log", log_dir, pid);
    if (ret < 0) {
        perror("snprintf failed for g_log_path");
        g_log_file = stdout;
        return;
    } else if ((size_t)ret >= sizeof(g_log_path)) {
        fprintf(stderr, "Error: Log file path truncated (required %d bytes, buffer %zu)\n",
                ret + 1, sizeof(g_log_path));
        g_log_file = stdout;
        return;
    }
    
    g_log_file = fopen(g_log_path, "a");
    if (g_log_file == NULL) {
        perror("fopen failed, falling back to stdout");
        g_log_file = stdout;
    }
}

static void __attribute__((destructor)) dlog_stub_fini(void)
{
    if (g_log_file != NULL && g_log_file != stdout) {
        fflush(g_log_file);
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

static FILE* GetLogFile()
{
    return g_log_file;
}

int32_t dlog_setlevel(int32_t moduleId, int32_t level, int32_t enableEvent)
{
    return 0;
}

int32_t CheckLogLevel(int32_t moduleId, int32_t logLevel)
{
    return logLevel >= g_global_log_level ? 1 : 0;
}

int32_t DlogSetAttr(LogAttr logAttrInfo)
{
    return 0;
}

static const char* GetLogLevelStr(int32_t level)
{
    switch (level) {
        case 3: return "ERROR";
        case 2: return "WARNING";
        case 1: return "INFO";
        case 0: return "DEBUG";
        default: return "UNKNOWN";
    }
}

void DlogVaList(int32_t moduleId, int32_t level, const char *fmt, va_list list)
{
    FILE* log_file = GetLogFile();
    if (log_file == NULL) {
        return;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm tm_buf;
    localtime_r(&ts.tv_sec, &tm_buf);

    char timestamp[64];
    size_t len = strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    snprintf(timestamp + len, sizeof(timestamp) - len, ".%03ld", ts.tv_nsec / 1000000);
    
    pthread_mutex_lock(&log_mutex);
    
    fprintf(log_file, "[%s] [%s] ", GetLogLevelStr(level), timestamp);
    vfprintf(log_file, fmt, list);
    fprintf(log_file, "\n");
    fflush(log_file);
    
    pthread_mutex_unlock(&log_mutex);
}

void DlogRecord(int32_t moduleId, int32_t level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DlogVaList(moduleId, level, fmt, args);
    va_end(args);
}