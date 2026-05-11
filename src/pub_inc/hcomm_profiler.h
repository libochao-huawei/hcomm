/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_PROFILER_H
#define HCOMM_PROFILER_H

#ifdef HCOMM_PROFILER_ENABLE

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

#include "log.h"

namespace hcomm {
namespace profiler {

inline uint64_t ReadTsc()
{
#if defined(__aarch64__)
    uint64_t value = 0;
    asm volatile("mrs %0, cntvct_el0" : "=r"(value));
    return value;
#elif defined(__x86_64__)
    uint32_t lo = 0;
    uint32_t hi = 0;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32U) | static_cast<uint64_t>(lo);
#else
    return 0;
#endif
}

inline uint64_t ReadCounterFrequency()
{
#if defined(__aarch64__)
    uint64_t value = 0;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
#elif defined(__x86_64__)
    std::ifstream cpuInfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuInfo, line)) {
        const std::string key = "cpu MHz";
        const std::size_t keyPos = line.find(key);
        if (keyPos == std::string::npos) {
            continue;
        }
        const std::size_t colonPos = line.find(':', keyPos + key.length());
        if (colonPos == std::string::npos) {
            continue;
        }
        const char* freqStr = line.c_str() + colonPos + 1U;
        const double mhz = std::atof(freqStr);
        if (mhz > 0.0) {
            return static_cast<uint64_t>(mhz * 1000000.0);
        }
    }
    return 0;
#else
    return 0;
#endif
}

inline uint32_t ReadTid()
{
    return static_cast<uint32_t>(syscall(SYS_gettid));
}

inline void Emit(char phase, const char* name)
{
    const char* safeName = (name == nullptr) ? "" : name;
    HCCL_RUN_INFO("[HCOMM_PROF]%c,%llu,%u,%s", phase,
        static_cast<unsigned long long>(ReadTsc()), ReadTid(), safeName);
}

inline void Init()
{
    static std::atomic<bool> inited(false);
    bool expected = false;
    if (!inited.compare_exchange_strong(expected, true)) {
        return;
    }

#if defined(__aarch64__)
    const char* arch = "arm64";
#elif defined(__x86_64__)
    const char* arch = "x86_64";
#else
    const char* arch = "unknown";
#endif

    HCCL_RUN_INFO("[HCOMM_PROF]F,%llu,0,%s",
        static_cast<unsigned long long>(ReadCounterFrequency()), arch);
}

class Scope {
public:
    explicit Scope(const char* name) : name_(name)
    {
        Emit('B', name_);
    }

    ~Scope()
    {
        Emit('E', name_);
    }

private:
    const char* name_;
};

}  // namespace profiler
}  // namespace hcomm

#define HCOMM_PROFILER_INIT() ::hcomm::profiler::Init()
#define HCOMM_ZONE_SCOPED() ::hcomm::profiler::Scope hcommProfilerScope_##__LINE__(__func__)
#define HCOMM_ZONE_SCOPED_N(name) ::hcomm::profiler::Scope hcommProfilerScope_##__LINE__(name)
#define HCOMM_ZONE_BEGIN(name) ::hcomm::profiler::Emit('B', name)
#define HCOMM_ZONE_END(name) ::hcomm::profiler::Emit('E', name)
#define HCOMM_ZONE_INSTANT(name) ::hcomm::profiler::Emit('I', name)

#else

#define HCOMM_PROFILER_INIT() ((void)0)
#define HCOMM_ZONE_SCOPED() ((void)0)
#define HCOMM_ZONE_SCOPED_N(name) ((void)0)
#define HCOMM_ZONE_BEGIN(name) ((void)0)
#define HCOMM_ZONE_END(name) ((void)0)
#define HCOMM_ZONE_INSTANT(name) ((void)0)

#endif

#endif
