/*
* Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
* Description: gradient Split Tune log def file
* Create: 2020-09-30
*/

#ifndef TUNE_LOG_H
#define TUNE_LOG_H

#include <unistd.h>
#include <stdio.h>

#define TUNE_ERROR(fmt, args...) \
    fprintf(stderr, "%s, pid(%d), %s(%d): " fmt "\n", __TIME__, getpid(), __func__, __LINE__, ##args)
#define TUNE_WARN(fmt, args...) \
    fprintf(stderr, "%s, pid(%d), %s(%d): " fmt "\n", __TIME__, getpid(), __func__, __LINE__, ##args)
#define TUNE_INFO(fmt, args...) \
    fprintf(stderr, "%s, pid(%d), %s(%d): " fmt "\n", __TIME__, getpid(), __func__, __LINE__, ##args)
#define TUNE_DEBUG(fmt, args...)  \
    fprintf(stderr, "%s, pid(%d), %s(%d): " fmt "\n", __TIME__, getpid(), __func__, __LINE__, ##args)
#define TUNE_EVENT(fmt, args...)  \
    fprintf(stderr, "%s, pid(%d), %s(%d): " fmt "\n", __TIME__, getpid(), __func__, __LINE__, ##args)
#define TUNE_CHK_RET(result, exeLog, retCode) do { \
    if (result) {                                                                       \
        exeLog;                                                                     \
        return retCode;                                               \
    }                                                                                \
} while (0)
#endif