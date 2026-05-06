/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string>
#include <vector>
#include <cstdint>

#include "log.h"

struct TimerEntry 
{
    uint64_t startTime;
    uint64_t endTime;
    uint32_t level;
    std::string name;

    TimerEntry(uint64_t startTime, uint32_t level, const std::string& name)
        : startTime(startTime), level(level), name(name) {}

    void Dump() const {
        HCCL_INFO("TimerEntry: level=%u, name=%s, startTime=%lu, endTime=%lu, duration=%lu",
                  level, name.c_str(), startTime, endTime, endTime - startTime);
    }
} ;

class Timer {
public:
    thread_local static uint64_t curLevel;
    thread_local static std::vector<TimerEntry> entries;
    static void DumpTimerLogs() {
        for (const auto& entry : entries) {
            entry.Dump();
        }
    }
    explicit Timer(const std::string& name)
    {
        curLevel++;
        timerIndex = entries.size();
        entries.emplace_back(GetCurrentTime(), curLevel, name);
    }
    ~Timer()
    {
        entries[timerIndex].endTime = GetCurrentTime();
        curLevel--;
    }

private:
    uint64_t GetCurrentTime() const {
        uint64_t cntvct;
        AsmCntvc(cntvct);
        return cntvct;
    }

    size_t timerIndex;
};

class TimerLogger {
public:
    TimerLogger()
    {
        Timer::entries.reserve(20000);
    };

    ~TimerLogger()
    {
        DumpLogs();
    }

    static TimerLogger& GetInstance() {
        static TimerLogger instance;
        return instance;
    }

    void Reset() {
        Timer::entries.clear();
        Timer::curLevel = 0;
    }

    void DumpLogs() {
        HCCL_INFO("Dumping Timer Logs, total entries: %zu", Timer::entries.size());
        Timer::DumpTimerLogs();
    }
};

thread_local uint64_t Timer::curLevel = 0;
thread_local std::vector<TimerEntry> Timer::entries;

#define PERF(name) TIMER_1(name, __COUNTER__)
#define TIMER_1(name, counter) Timer timer##counter(name)

#define FUNC_PERF FUNC_PERF_1(__COUNTER__)
#define FUNC_PERF_1(counter) Timer func_timer##counter(CURRENT_FUNCTION)
#define STRINGIFY(x) #x
#define CURRENT_FUNCTION __FILE__ ":" STRINGIFY(__func__)