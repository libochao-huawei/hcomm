#ifndef PERF_TIMER_H
#define PERF_TIMER_H

#include <vector>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <algorithm>
#include "thread_timer.h"
// 单次函数调用耗时记录
namespace Hccl {
struct TimeEntry {
    std::string func_name;
    long long   duration_us;
};

struct TimerResult {
    std::string name;
    long long   duration_ms;
};

struct SingleOperatorHostResult {
    std::string name;
    long long   hccl_duration_ms;
    long long   runtime_duration_ms;
    long long   hccl_cpu_duration_s;
    long long   runtime_cpu_duration_s; 
};

// 声明全局存储和锁（定义在.cpp中）
// extern std::vector<TimeEntry> time_entries;
// extern std::vector<TimerResult> results;
// extern std::mutex entries_mutex;
// extern std::mutex  Timer_mutex;

// RAII计时器类
class FunctionTimer {
public:
    explicit FunctionTimer(const std::string &name);
    ~FunctionTimer();

private:
    std::string                                                 func_name_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};
class Timer {
public:
    explicit Timer(const std::string &name);
    void stop();

private:
    std::string                                                 name_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

class SingleOperatorHostTimer {
public:
    explicit SingleOperatorHostTimer(const std::string &name)
        : name_(name), SingleOperator_(ThreadTimer::Create()),
          runtimeTime_(ThreadTimer::Create())
    {
    }
    void start()
    {
        stopTime_ = 0;
        running_ = true;
        SingleOperator_.StartTimer();
    }
    void stop();
    ~SingleOperatorHostTimer()
    {
        if (running_ == true && ++stopTime_ == 1) {
            stop();
        }
    }
    // runtime time add
    void SetIterationTime(double seconds)
    {
        (void)seconds;
        throw std::runtime_error("Not implemented");
    }
    std::string name_;
    ThreadTimer SingleOperator_;
    ThreadTimer runtimeTime_;
    uint16_t   stopTime_         = 0;
    bool running_ = false;
};

inline double notRAIIStart(double& cputime){
    cputime = ThreadCPUUsage();
    return ChronoClockNow();
}
void notRaIIstop(double start, double cputime, const std::string& printName);
#define NOTRAIISTOP(timer, cputimer) notRaIIstop(timer, cputimer, #timer)
// // 宏简化函数计时（直接使用 __func__ 或自定义名称）
// #define FUNC_TIMER FunctionTimer timer(__func__);
// #define FUNC_TIMER_NAMED(name) FunctionTimer timer(name);
// // 定义计时宏（确保在同一作用域内使用相同的name）
// #define TIMER_START(name) Timer MACRO_CONCAT(__timer_, name)(#name)
// #define TIMER_END(name) MACRO_CONCAT(__timer_, name).stop()
// #define MACRO_CONCAT(a, b) a##b
// #define SINGLE_OPERATOR_TIMER_START(name) SingleOperatorHostTimer MACRO_CONCAT(__timer_, name)(#name)
// #define SINGLE_OPERATOR_TIMER_END(name) MACRO_CONCAT(__timer_, name).stop()
} // namespace Hccl
#endif