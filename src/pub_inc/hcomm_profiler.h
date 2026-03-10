/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * @file hcomm_profiler.h
 * @brief 轻量化性能打点库，基于 HCCL 自研 Log 落地，离线转换为 Tracy 可视化格式
 *
 * 设计原则：
 *   1. 零网络依赖：所有数据通过 HCCL_RUN_INFO / HCCL_INFO 落盘，走华为官方日志导出通道
 *   2. 极低开销：ARM64 使用 CNTVCT_EL0 硬件寄存器，x86 使用 RDTSC，无系统调用
 *   3. 编译开关：通过 HCOMM_PROFILER_ENABLE 宏控制，未定义时所有宏为空操作
 *   4. Chrome Tracing 兼容：日志格式可被 Tracy import-chrome 工具直接转换为 .tracy 文件
 *
 * 使用方法：
 *   #define HCOMM_PROFILER_ENABLE  // 在需要打点的编译单元或CMake中定义
 *   #include "hcomm_profiler.h"
 *
 *   void MyFunction() {
 *       HCOMM_ZONE_SCOPED();                    // 自动以 __FUNCTION__ 命名
 *       HCOMM_ZONE_BEGIN("CustomName");          // 手动 Begin
 *       // ... your code ...
 *       HCOMM_ZONE_END("CustomName");            // 手动 End
 *       HCOMM_ZONE_INSTANT("SomeEvent");         // 瞬时事件标记
 *   }
 *
 * 日志格式 (每条日志一行，前缀为 [HCOMM_PROF])：
 *   [HCOMM_PROF]<ph>,<tsc>,<tid>,<name>
 *   其中 ph: B(Begin)/E(End)/I(Instant), tsc: 硬件时间戳, tid: 线程ID
 */

#ifndef HCOMM_PROFILER_H
#define HCOMM_PROFILER_H

#ifdef HCOMM_PROFILER_ENABLE

#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

/* ========================================================================
 * 1. 高精度硬件时间戳获取 (参考 Tracy Profiler GetTime() 实现)
 * ======================================================================== */

/**
 * @brief 获取高精度硬件时间戳（零系统调用开销）
 * ARM64 (AICPU/昇腾): CNTVCT_EL0，频率通常 50MHz
 * x86_64 (Host 侧): RDTSC，频率跟随 CPU TSC
 */
static inline uint64_t HcommGetTsc(void)
{
    uint64_t val;
#if defined(__aarch64__)
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
#elif defined(__x86_64__) || defined(_M_X64)
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    val = ((uint64_t)hi << 32) | lo;
#else
    /* 降级方案：使用 clock_gettime */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    val = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
    return val;
}

/**
 * @brief 获取硬件时钟频率 (用于离线转换时将 cycle 数转为微秒)
 * ARM64: 读取 CNTFRQ_EL0 寄存器
 * x86: 需要外部校准（Python 脚本侧处理）
 */
static inline uint64_t HcommGetTscFreq(void)
{
    uint64_t freq = 0;
#if defined(__aarch64__)
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(freq));
#endif
    return freq; /* x86 返回 0，由离线脚本通过 /proc/cpuinfo 获取 */
}

/* ========================================================================
 * 2. 日志输出层（集成 HCCL 自研日志库）
 * ======================================================================== */

/*
 * 日志格式规范（Chrome Tracing 兼容）：
 * [HCOMM_PROF]<phase>,<timestamp>,<thread_id>,<zone_name>
 *
 * phase: B = Zone Begin, E = Zone End, I = Instant event
 *        F = Frequency calibration (仅在 Init 时输出一次)
 *
 * 示例输出（真实日志中会被 HCCL_INFO 添加文件名和行号前缀）：
 *   [HCOMM_PROF]F,50000000,0,ARM64_CNTVCT
 *   [HCOMM_PROF]B,839201822719,1001,LoadOffloadCollOp
 *   [HCOMM_PROF]E,839201855034,1001,LoadOffloadCollOp
 *   [HCOMM_PROF]I,839201860000,1001,StreamSyncDone
 */

/* 使用 HCCL_RUN_INFO 以确保日志写入运行日志（run log），不受 DEBUG 级别过滤 */
#define HCOMM_PROF_LOG(phase, name) \
    HCCL_RUN_INFO("[HCOMM_PROF]%c,%lu,%u,%s", \
        (phase), (unsigned long)HcommGetTsc(), (unsigned int)syscall(SYS_gettid), (name))

/* ========================================================================
 * 3. 用户打点宏
 * ======================================================================== */

/** 手动标记 Zone 开始 */
#define HCOMM_ZONE_BEGIN(name) HCOMM_PROF_LOG('B', name)

/** 手动标记 Zone 结束 */
#define HCOMM_ZONE_END(name) HCOMM_PROF_LOG('E', name)

/** 标记瞬时事件 */
#define HCOMM_ZONE_INSTANT(name) HCOMM_PROF_LOG('I', name)

/**
 * @brief RAII 自动 Zone（构造时 Begin，析构时 End）
 * 使用方法: HCOMM_ZONE_SCOPED();  // 区域名自动取 __FUNCTION__
 *          HCOMM_ZONE_SCOPED_N("MyCustomName");
 */
struct HcommProfileGuard {
    const char *m_name;
    HcommProfileGuard(const char *name) : m_name(name) { HCOMM_PROF_LOG('B', m_name); }
    ~HcommProfileGuard() { HCOMM_PROF_LOG('E', m_name); }
    HcommProfileGuard(const HcommProfileGuard &) = delete;
    HcommProfileGuard &operator=(const HcommProfileGuard &) = delete;
};

#define HCOMM_ZONE_SCOPED() HcommProfileGuard hcomm_zone_guard_##__LINE__(__FUNCTION__)
#define HCOMM_ZONE_SCOPED_N(name) HcommProfileGuard hcomm_zone_guard_##__LINE__(name)

/**
 * @brief 输出频率校准信息（需在程序初始化时调用一次）
 * 离线脚本读取此行来计算 tsc -> 微秒 的换算因子
 */
#define HCOMM_PROFILER_INIT() do { \
    uint64_t _freq = HcommGetTscFreq(); \
    if (_freq > 0) { \
        HCCL_RUN_INFO("[HCOMM_PROF]F,%lu,0,ARM64_CNTVCT", (unsigned long)_freq); \
    } else { \
        HCCL_RUN_INFO("[HCOMM_PROF]F,0,0,X86_RDTSC"); \
    } \
} while(0)

#else /* !HCOMM_PROFILER_ENABLE */

/* 未使能时，所有宏展开为空操作，零开销 */
#define HCOMM_ZONE_BEGIN(name)       ((void)0)
#define HCOMM_ZONE_END(name)         ((void)0)
#define HCOMM_ZONE_INSTANT(name)     ((void)0)
#define HCOMM_ZONE_SCOPED()          ((void)0)
#define HCOMM_ZONE_SCOPED_N(name)    ((void)0)
#define HCOMM_PROFILER_INIT()        ((void)0)

#endif /* HCOMM_PROFILER_ENABLE */

#endif /* HCOMM_PROFILER_H */
