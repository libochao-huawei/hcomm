#include "timer.h"
#ifdef CCL_KERNEL_AICPU
    uint64_t HcclTimerAICPU::timerCounterAicpu = 0;
    std::vector<TimerEntryAICPU> HcclTimerAICPU::timerEntriesAicpu;
    static HcclTimerDumperAICPU g_TimerDumper_aicpu;
#endif