#include "log.h"
#include "perf_timer.h"

namespace Hccl {

class PrintTime {
public:
    PrintTime()
    {
        constexpr std::size_t DEFAULT_SIZE = 1024 * 1024;
        singleOperatorHostResult.reserve(DEFAULT_SIZE);
    }
    ~PrintTime()
    {
        printResults();
        print_timing_stats();
        printSingleOperatorHostResult();
    }
    void printResults()
    {
        for (const auto &res : results) {
            HCCL_ERROR(("[" + res.name + "] " + std::to_string(res.duration_ms) + "ns").c_str());
        }
        results.clear();
    }

    void printSingleOperatorHostResult()
    {
        for (const auto &res : singleOperatorHostResult) {
            HCCL_ERROR("FUN: %s, Total:[%lld]us, hccl:[%lld]us, runtime:[%lld], hcclcpu:[%lld]us, runtimecpu:[%lld]us",
                       res.name.c_str(),
                       res.hccl_duration_ms + res.runtime_duration_ms, res.hccl_duration_ms,
                       res.runtime_duration_ms, res.hccl_cpu_duration_s, res.runtime_cpu_duration_s);
        }
        singleOperatorHostResult.clear();
    }
    // 统计并输出结果
    void print_timing_stats()
    {
        std::unordered_map<std::string, long long> func_time_map;

        // 汇总数据到map
        for (const auto &entry : time_entries) {
            auto it = func_time_map.find(entry.func_name);
            if (it == func_time_map.end()) {
                func_time_map.emplace(entry.func_name, entry.duration_us);
            } else {
                it->second += entry.duration_us;
            }
        }
        // 排序
        std::vector<std::pair<std::string, long long>> sorted_stats(func_time_map.begin(), func_time_map.end());
        std::sort(sorted_stats.begin(), sorted_stats.end(), [](const auto &a, const auto &b) {
            return a.second > b.second;
        });

        // 输出
        // std::cout << "Function execution time (microseconds):\n";
        for (const auto &entry : sorted_stats) {
            const std::string &name = entry.first;
            const long long   &time = entry.second;
            // std::cout << "  " << name << ": " << time << " μs\n";
            HCCL_ERROR(("[" + name + "] " + std::to_string(time) + "ns").c_str());
        }
        time_entries.clear();
    }
    std::vector<TimeEntry>                time_entries;
    std::vector<TimerResult>              results;
    std::vector<SingleOperatorHostResult> singleOperatorHostResult;
};
static thread_local PrintTime printTime{};

// 定义全局变量
FunctionTimer::FunctionTimer(const std::string &name)
    : func_name_(name), start_(std::chrono::high_resolution_clock::now())
{
}

FunctionTimer::~FunctionTimer()
{
    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
    printTime.time_entries.push_back({func_name_, duration.count()});

}

// 计时结果结构体

Timer::Timer(const std::string &name) : name_(name)
{
    start_ = std::chrono::high_resolution_clock::now();
}

void Timer::stop()
{
    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
    printTime.results.push_back({name_, duration});

}

void SingleOperatorHostTimer::stop()
{
    SingleOperator_.StopTimer();
    ++stopTime_;
    long long timens = (SingleOperator_.real_time_used() - runtimeTime_.real_time_used()) * 1e6;
    printTime.singleOperatorHostResult.emplace_back(SingleOperatorHostResult{
            name_, timens,
            static_cast<long long>(runtimeTime_.real_time_used() * 1e6), 
            static_cast<long long>(SingleOperator_.cpu_time_used() * 1e6),
            static_cast<long long>(runtimeTime_.cpu_time_used() * 1e6)});
}

void notRaIIstop(double start, double cputime, const std::string& printName)
{
    long long timens = (ChronoClockNow() - start) * 1e6;
    long long cputime_s = ThreadCPUUsage() - cputime;
    printTime.singleOperatorHostResult.emplace_back(SingleOperatorHostResult{
        printName, timens, 0, cputime_s, 0});
}

} // namespace Hccl