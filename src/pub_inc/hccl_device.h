#ifndef HCCL_DEVICE_LOG_H
#define HCCL_DEVICE_LOG_H
 
extern "C" {
    __attribute__((visibility("default"))) void logC(const char *func, const char *file, const int line,
                                                     const char *type, const char *format, ...);
}
#endif