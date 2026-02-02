#include <linux/time.h>
void *jiffies;



void __const_udelay(unsigned long xloops)
{

}

void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * 0x000010c7); /* 2**32 / 1000000 (rounded up) */
}

void do_gettimeofday(struct timeval *tv)
{
}

void ktime_get_real_ts64(struct timespec64 *ts)
{
	ts->tv_sec = 1546300810;
	ts->tv_nsec = 0;
}

