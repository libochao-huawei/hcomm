#include "ut_lib.h"

#include <linux/mutex.h>

static UT_MAP_DEFINE(mutex_lock, trace);
int stub_mutex_error()
{
	return UT_MAP_CLR(mutex_lock, trace);
}

void
__mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key)
{
	(void)UT_MAP_CLR(mutex_lock, trace);
}
EXPORT_SYMBOL(__mutex_init);


/**
 * mutex_lock - acquire the mutex
 * @lock: the mutex to be acquired
 *
 * Lock the mutex exclusively for this task. If the mutex is not
 * available right now, it will sleep until it can get it.
 *
 * The mutex must later on be released by the same task that
 * acquired it. Recursive locking is not allowed. The task
 * may not exit without first unlocking the mutex. Also, kernel
 * memory where the mutex resides must not be freed with
 * the mutex still locked. The mutex must first be initialized
 * (or statically defined) before it can be locked. memset()-ing
 * the mutex to 0 is not allowed.
 *
 * (The CONFIG_DEBUG_MUTEXES .config option turns on debugging
 * checks that will enforce the restrictions and will also do
 * deadlock debugging)
 *
 * This function is similar to (but not equivalent to) down().
 */
void __mutex_lock(struct mutex *lock, const char *file, int line)
{
	void *ptr = ut_malloc_trace(sizeof(*lock), 0, 0, file, line, "LOCK"); 

	(void)UT_MAP_INSERT(mutex_lock, trace, lock, ptr);
}
EXPORT_SYMBOL(__mutex_lock);



/**
 * mutex_unlock - release the mutex
 * @lock: the mutex to be released
 *
 * Unlock a mutex that has been locked by this task previously.
 *
 * This function must not be used in interrupt context. Unlocking
 * of a not locked mutex is not allowed.
 *
 * This function is similar to (but not equivalent to) up().
 */
void __mutex_unlock(struct mutex *lock, const char *file, int line)
{
	void *ptr = UT_MAP_ERASE(mutex_lock, trace, lock); 
	if (ptr)
		ut_free_trace(ptr, 0, file, line, NULL);
}
EXPORT_SYMBOL(__mutex_unlock);


/**
 * mutex_trylock - try to acquire the mutex, without waiting
 * @lock: the mutex to be acquired
 *
 * Try to acquire the mutex atomically. Returns 1 if the mutex
 * has been acquired successfully, and 0 on contention.
 *
 * NOTE: this function follows the spin_trylock() convention, so
 * it is negated from the down_trylock() return values! Be careful
 * about this when converting semaphore users to mutexes.
 *
 * This function must not be used in interrupt context. The
 * mutex must be released by the same task that acquired it.
 */
UT_CNT_RANGE_DEFINE(mutex_trylock, fail);
int __mutex_trylock(struct mutex *lock, const char *file, int line)
{
	void *ptr;

	if (UT_CNT_RANGE_CHECK(mutex_trylock, fail, 1))
		return 0;
	
	ptr = ut_malloc_trace(sizeof(*lock), 0, 0, file, line, "TRY-LOCK");
	(void)UT_MAP_INSERT(mutex_lock, trace, lock, ptr);

	return 1;
}
EXPORT_SYMBOL(__mutex_trylock);

UT_CNT_RANGE_DEFINE(wait_for_completion_timeout, always);
unsigned long wait_for_completion_timeout(struct completion *x,
						   unsigned long timeout)
{
	if (UT_CNT_RANGE_CHECK(wait_for_completion_timeout, always, 1))
		return 0;

	return 1;
}
