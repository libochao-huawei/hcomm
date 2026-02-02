#include <linux/kernel_stat.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/pid_namespace.h>
#include <linux/notifier.h>
#include <linux/thread_info.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/posix-timers.h>
#include <linux/cpu.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/tick.h>
#include <linux/kallsyms.h>
#include <linux/irq_work.h>


unsigned long volatile __jiffy_data jiffies;

void __const_udelay(unsigned long xloops)
{

}

void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * 0x000010c7); /* 2**32 / 1000000 (rounded up) */
}

static void llt_jiffies_clock_func(void *data) {
	jiffies += HZ;
}

int llt_register_clock(void (*fn)(void *), int nsec, void *data, char *name);
static int __init llt_time_stub_init(void)
{
	llt_register_clock(llt_jiffies_clock_func, 1, NULL, "jiffies_1second");
	return 0;
}
late_initcall(llt_time_stub_init);

ktime_t ktime_get(void)
{

}
void do_gettimeofday(struct timeval *tv)
{
	tv->tv_sec = 1546300810;
        tv->tv_usec = 0;
}

void ktime_get_real_ts64(struct timespec64 *ts)
{
	ts->tv_sec = 1546300810;
	ts->tv_nsec = 0;
}
