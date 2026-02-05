#include "timers.h"

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h> // this header must be included before 'sys/sysctl.h' to avoid compilation error on FreeBSD
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits>
#include <mutex>

namespace Hccl {

// Suppress unused warnings on helper functions.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if __has_feature(cxx_attributes)
#define BENCHMARK_NORETURN [[noreturn]]
#elif defined(__GNUC__)
#define BENCHMARK_NORETURN __attribute__((noreturn))
#else
#define BENCHMARK_NORETURN
#endif

namespace {
double MakeTime(struct rusage const &ru)
{
    return (static_cast<double>(ru.ru_utime.tv_sec) + static_cast<double>(ru.ru_utime.tv_usec) * 1e-6
            + static_cast<double>(ru.ru_stime.tv_sec) + static_cast<double>(ru.ru_stime.tv_usec) * 1e-6);
}
#if defined(CLOCK_PROCESS_CPUTIME_ID) || defined(CLOCK_THREAD_CPUTIME_ID)
double MakeTime(struct timespec const &ts)
{
    return static_cast<double>(ts.tv_sec) + (static_cast<double>(ts.tv_nsec) * 1e-9);
}
#endif

BENCHMARK_NORETURN void DiagnoseAndExit(const char *msg)
{
    std::cerr << "ERROR: " << msg << '\n';
    std::flush(std::cerr);
    std::exit(EXIT_FAILURE);
}

} // end namespace

double ProcessCPUUsage()
{
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    // FIXME We want to use clock_gettime, but its not available in MacOS 10.11.
    // See https://github.com/google/benchmark/pull/292
    struct timespec spec {};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &spec) == 0) {
        return MakeTime(spec);
    }
    DiagnoseAndExit("clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ...) failed");
#else
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0)
        return MakeTime(ru);
    DiagnoseAndExit("getrusage(RUSAGE_SELF, ...) failed");
#endif
}

double ThreadCPUUsage()
{
#if defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts {};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
        return MakeTime(ts);
    }
    DiagnoseAndExit("clock_gettime(CLOCK_THREAD_CPUTIME_ID, ...) failed");
#else
#error Per-thread timing is not available on your system.
#endif
}

std::string LocalDateTimeString()
{
    // Write the local time in RFC3339 format yyyy-mm-ddTHH:MM:SS+/-HH:MM.
    typedef std::chrono::system_clock Clock;
    std::time_t                       now           = Clock::to_time_t(Clock::now());
    const std::size_t                 kTzOffsetLen  = 6;
    const std::size_t                 kTimestampLen = 19;

    std::size_t tz_len         = 0;
    std::size_t timestamp_len  = 0;
    long int    offset_minutes = 0;
    char        tz_offset_sign = '+';
    // tz_offset is set in one of three ways:
    // * strftime with %z - This either returns empty or the ISO 8601 time.  The
    // maximum length an
    //   ISO 8601 string can be is 7 (e.g. -03:30, plus trailing zero).
    // * snprintf with %c%02li:%02li - The maximum length is 41 (one for %c, up to
    // 19 for %02li,
    //   one for :, up to 19 %02li, plus trailing zero).
    // * A fixed string of "-00:00".  The maximum length is 7 (-00:00, plus
    // trailing zero).
    //
    // Thus, the maximum size this needs to be is 41.
    char tz_offset[41];
    // Long enough buffer to avoid format-overflow warnings
    char storage[128];

    std::tm  timeinfo{};
    std::tm *timeinfo_p = &timeinfo;
    ::localtime_r(&now, &timeinfo);

    tz_len = std::strftime(tz_offset, sizeof(tz_offset), "%z", timeinfo_p);

    if (tz_len < kTzOffsetLen && tz_len > 1) {
        // Timezone offset was written. strftime writes offset as +HHMM or -HHMM,
        // RFC3339 specifies an offset as +HH:MM or -HH:MM. To convert, we parse
        // the offset as an integer, then reprint it to a string.

        offset_minutes = ::strtol(tz_offset, NULL, 10);
        if (offset_minutes < 0) {
            offset_minutes *= -1;
            tz_offset_sign = '-';
        }

        tz_len = static_cast<size_t>(::snprintf(tz_offset, sizeof(tz_offset), "%c%02li:%02li", tz_offset_sign,
                                                offset_minutes / 100, offset_minutes % 100));
        ((void)tz_len); // Prevent unused variable warning in optimized build.
    } else {
        // Unknown offset. RFC3339 specifies that unknown local offsets should be
        // written as UTC time with -00:00 timezone.
        ::gmtime_r(&now, &timeinfo);
        strncpy(tz_offset, "-00:00", kTzOffsetLen + 1);
    }

    timestamp_len = std::strftime(storage, sizeof(storage), "%Y-%m-%dT%H:%M:%S", timeinfo_p);
    // Prevent unused variable warning in optimized build.
    ((void)kTimestampLen);

    std::strncat(storage, tz_offset, sizeof(storage) - timestamp_len - 1);
    return std::string(storage);
}

} // end namespace Hccl
