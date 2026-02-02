#ifndef _STUB_LOCK_H_
#define _STUB_LOCK_H_

int stub_spinlock_error();
int stub_mutex_error();

extern UT_CNT_RANGE_DECLARE(mutex_trylock, fail);
extern UT_CNT_RANGE_DECLARE(wait_for_completion_timeout, always);
extern UT_CNT_RANGE_DECLARE(create_singlethread_workqueue, fail);

#endif
