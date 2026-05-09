#ifndef HCCL_TIMER_H
#define HCCL_TIMER_H
#include <string>
#include <vector>
#include <sys/time.h> /* 获取时间 */
#include "log.h"

#ifdef CCL_KERNEL_AICPU

/*================= 性能打点 ===================*/
struct TimerEntryAICPU {
    uint64_t startTime;
    uint64_t endTime;
    uint32_t timerLevel;
    std::string name;
 
    TimerEntryAICPU(uint64_t startTime, uint32_t timerLevel, const std::string &name) :
        startTime(startTime), timerLevel(timerLevel), name(name) {
    }
 
    void PrintLog()
    {
        uint64_t elapsedNano = endTime - startTime;
        HCCL_ERROR("TIMER: Level: %lu, Timer: %s, Start: %llu, End: %llu, Duration: %zu ns",
            timerLevel, name.c_str(), startTime, endTime, elapsedNano);
    }
};
 
class HcclTimerAICPU {
  public:
    static uint64_t timerCounterAicpu;
    static std::vector<TimerEntryAICPU> timerEntriesAicpu;
    uint64_t GetCurAicpuTimestamp()
    {
        struct timespec timestamp;
        (void)clock_gettime(1, &timestamp);
        return static_cast<uint64_t>((timestamp.tv_sec * 1000000000U) + (timestamp.tv_nsec));
    }
    explicit HcclTimerAICPU(const std::string &name)
    {
        timerCounterAicpu++;
        timerIdx = timerEntriesAicpu.size();
        timerEntriesAicpu.emplace_back(GetCurAicpuTimestamp(), timerCounterAicpu, name);
    }
 
    ~HcclTimerAICPU()
    {
        timerEntriesAicpu[timerIdx].endTime = GetCurAicpuTimestamp();
        timerCounterAicpu--;
    }
 
    static void DumpTimerLogs() {
        HCCL_ERROR("Dump timer log %s",  std::to_string(timerEntriesAicpu.size()).c_str());
        for (auto &entry : timerEntriesAicpu) {
            entry.PrintLog();
        }
        timerEntriesAicpu.clear();
    }
 
  private:
    size_t timerIdx;
};
 
class HcclTimerDumperAICPU {
  public:
    HcclTimerDumperAICPU() {
        HCCL_ERROR("Dump timer");
        HcclTimerAICPU::timerEntriesAicpu.reserve(20000);
    };

    static HcclTimerDumperAICPU &GetInstance()
    {
        static HcclTimerDumperAICPU instance;
        return instance;
    }

    void DumpTimerLogs()
    {
        HCCL_ERROR("Dump timer logs");
        HcclTimerAICPU::DumpTimerLogs();
    }

    ~HcclTimerDumperAICPU()
    {
        DumpTimerLogs();
    }
};
#define CURRENT_FUNCTION_LOCATION_AICPU std::string(__FILE__) + ":" + std::string(__func__)
#define MY_TIMER2_AICPU(name, counter) HcclTimerAICPU myTimer##counter(name)
#define MY_TIMER1_AICPU(name, counter) MY_TIMER2_AICPU(name, counter)
#define MY_TIMER_AICPU(name) MY_TIMER1_AICPU(name, __COUNTER__)
#define FUNCTION_TRACE_COUNTER2_AICPU(counter) HcclTimerAICPU timer##counter(CURRENT_FUNCTION_LOCATION_AICPU)
#define FUNCTION_TRACE_COUNTER_AICPU(counter) FUNCTION_TRACE_COUNTER2_AICPU(counter)
#define FUNCTION_TRACE_AICPU FUNCTION_TRACE_COUNTER_AICPU(__COUNTER__)

#endif
#endif // HCCL_TIMER_H