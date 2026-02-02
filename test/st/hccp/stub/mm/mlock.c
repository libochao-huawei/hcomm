#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/export.h>

bool can_do_mlock(void)
{
	return true;
}
EXPORT_SYMBOL(can_do_mlock);
