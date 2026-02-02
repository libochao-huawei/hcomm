#include "ut_lib.h"

#include <linux/gfp.h>
#include <linux/slab.h>

UT_CNT_RANGE_DEFINE(kmalloc, fail);
void *___kmalloc(size_t size, gfp_t gfp, const char *file, int line)
{
	int flags = 0;

	if (UT_CNT_RANGE_CHECK(kmalloc, fail, 1))
		return NULL;

	if (__GFP_ZERO & gfp)
		flags = UT_MEM_FLAG_ZERO;

	return ut_malloc_trace(size, 0, flags, file, line, "SLAB-MEM");
}

void *__kmalloc_track_caller(size_t size, gfp_t flags, unsigned long caller)
{
	return __kmalloc(size, flags);
}
EXPORT_SYMBOL(__kmalloc_track_caller);

void __kfree(const void *p, const char *file, int line)
{
	ut_free_trace(p, file, line, "kfree");
}

void * vmalloc(unsigned long size)
{
	return ut_malloc_trace(size, 0, 0, __FILE__, __LINE__, "vmalloc");
}

