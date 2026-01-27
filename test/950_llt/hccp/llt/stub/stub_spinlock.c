#include "ut_lib.h"

#include <linux/spinlock.h>

static int tc_spinlock_cnt = 0;
static int tc_spinunlock_cnt = 0;

void _spin_lock(spinlock_t *lock, const char *file, int line)
{
	tc_spinlock_cnt++;
}

int _spin_trylock(spinlock_t *lock, const char *file, int line)
{
	tc_spinlock_cnt++;
	return 1;
}

void _spin_unlock(spinlock_t *lock, const char *file, int line)
{
	tc_spinunlock_cnt++;
}

int stub_spinlock_error()
{
	int err = 0;
	
	if (tc_spinlock_cnt != tc_spinunlock_cnt) {
		ut_printf("[SPINLOCK-ERR] lock count = %d, unlock count = %d", tc_spinlock_cnt, tc_spinunlock_cnt);
		err++;
	}

	tc_spinlock_cnt = tc_spinunlock_cnt = 0;

	return err;
}

