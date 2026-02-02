#include "stdio.h"

#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/posix-timers.h>

/*
 * The timezone where the local system is located.  Used as a default by some
 * programs who obtain this value by using gettimeofday.
 */
struct timezone sys_tz;

EXPORT_SYMBOL(sys_tz);

/**
 * mod_timer - modify a timer's timeout
 * @timer: the timer to be modified
 * @expires: new timeout in jiffies
 *
 * mod_timer() is a more efficient way to update the expire field of an
 * active timer (if the timer is inactive it will be activated)
 *
 * mod_timer(timer, expires) is equivalent to:
 *
 *     del_timer(timer); timer->expires = expires; add_timer(timer);
 *
 * Note that if there are multiple unserialized concurrent users of the
 * same timer, then mod_timer() is the only safe way to modify the timeout,
 * since add_timer() cannot modify an already running timer.
 *
 * The function returns whether it has modified a pending timer or not.
 * (ie. mod_timer() of an inactive timer returns 0, mod_timer() of an
 * active timer returns 1.)
 */
int mod_timer(struct timer_list *timer, unsigned long expires)
{
	return 0;
}
EXPORT_SYMBOL(mod_timer);

/**
 * init_timer_key - initialize a timer
 * @timer: the timer to be initialized
 * @func: timer callback function
 * @flags: timer flags
 * @name: name of the timer
 * @key: lockdep class key of the fake lock used for tracking timer
 *       sync lock dependencies
 *
 * init_timer_key() must be done to a timer prior calling *any* of the
 * other timer functions.
 */
void init_timer_key(struct timer_list *timer, unsigned int flags,
                    const char *name, struct lock_class_key *key)
{

}
EXPORT_SYMBOL(init_timer_key);


/**
 * del_timer_sync - deactivate a timer and wait for the handler to finish.
 * @timer: the timer to be deactivated
 *
 * This function only differs from del_timer() on SMP: besides deactivating
 * the timer it also makes sure the handler has finished executing on other
 * CPUs.
 *
 * Synchronization rules: Callers must prevent restarting of the timer,
 * otherwise this function is meaningless. It must not be called from
 * interrupt contexts unless the timer is an irqsafe one. The caller must
 * not hold locks which would prevent completion of the timer's
 * handler. The timer's handler must not call add_timer_on(). Upon exit the
 * timer is not queued and the handler is not running on any CPU.
 *
 * Note: For !irqsafe timers, you must not hold locks that are held in
 *   interrupt context while calling this function. Even if the lock has
 *   nothing to do with the timer in question.  Here's why:
 *
 *    CPU0                             CPU1
 *    ----                             ----
 *                                   <SOFTIRQ>
 *                                   call_timer_fn();
 *                                     base->running_timer = mytimer;
 *  spin_lock_irq(somelock);
 *                                     <IRQ>
 *                                        spin_lock(somelock);
 *  del_timer_sync(mytimer);
 *   while (base->running_timer == mytimer);
 *
 * Now del_timer_sync() will never return and never release somelock.
 * The interrupt on the other CPU is waiting to grab somelock but
 * it has interrupted the softirq that CPU0 is waiting to finish.
 *
 * The function returns whether it has deactivated a pending timer or not.
 */
int del_timer_sync(struct timer_list *timer)
{
	return 0;
}
EXPORT_SYMBOL(del_timer_sync);

void add_timer(struct timer_list *timer)
{
        return 0;
}
EXPORT_SYMBOL(add_timer);

void add_timer_on(struct timer_list *timer, int cpu)
{
        return;
}
EXPORT_SYMBOL(add_timer_on);
/**
 * usleep_range - Sleep for an approximate time
 * @min: Minimum time in usecs to sleep
 * @max: Maximum time in usecs to sleep
 *
 * In non-atomic context where the exact wakeup time is flexible, use
 * usleep_range() instead of udelay().  The sleep improves responsiveness
 * by avoiding the CPU-hogging busy-wait of udelay(), and the range reduces
 * power usage by allowing hrtimers to take advantage of an already-
 * scheduled interrupt instead of scheduling a new one just for this sleep.
 */
void usleep_range(unsigned long min, unsigned long max)
{

}
EXPORT_SYMBOL(usleep_range);


/**
 * msleep - sleep safely even with waitqueue interruptions
 * @msecs: Time in milliseconds to sleep for
 */
void msleep(unsigned int msecs)
{

}
EXPORT_SYMBOL(msleep);

unsigned int jiffies_to_msecs(const unsigned long j)
{
	return 0;
};
EXPORT_SYMBOL(jiffies_to_msecs);

unsigned long round_jiffies_relative(unsigned long j)
{
	return 1;
}
EXPORT_SYMBOL_GPL(round_jiffies_relative);
