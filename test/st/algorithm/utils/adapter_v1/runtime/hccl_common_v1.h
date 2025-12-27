#ifndef HCCL_COMMON_V1_H
#define HCCL_COMMON_V1_H    

#include "dtype_common.h"

typedef struct {
    uint32_t addrOffset;
    uint32_t dataOffset;
} rtPlaceHolderInfo_t;

typedef struct tagRtDevBinary {
    uint32_t magic;
    uint32_t version;
    const void *data;
    uint64_t length;
} rtDevBinary_t;

typedef enum tagRtStreamCaptureStatus {
    RT_STREAM_CAPTURE_STATUS_NONE   = 0,
    RT_STREAM_CAPTURE_STATUS_ACTIVE = 1,
    RT_STREAM_CAPTURE_STATUS_INVALIDATED = 2,
    RT_STREAM_CAPTURE_STATUS_MAX
} rtStreamCaptureStatus;

#endif
