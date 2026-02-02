#include "ut_lib.h"

#include <linux/gfp.h>
#include <linux/device.h>


/**
 * devm_kmalloc - Resource-managed kmalloc
 * @dev: Device to allocate memory for
 * @size: Allocation size
 * @gfp: Allocation gfp flags
 *
 * Managed kmalloc.  Memory allocated with this function is
 * automatically freed on driver detach.  Like all other devres
 * resources, guaranteed alignment is unsigned long long.
 *
 * RETURNS:
 * Pointer to allocated memory on success, NULL on failure.
 */
UT_CNT_RANGE_DEFINE(__devm_kmalloc, fail);
void *__devm_kmalloc(struct device *dev, size_t size, gfp_t gfp, const char *file, int line)
{
	int flags = 0;
	
	if (UT_CNT_RANGE_CHECK(__devm_kmalloc, fail, 1))
		return NULL;

	if (__GFP_ZERO & gfp)
		flags = UT_MEM_FLAG_ZERO;
	
	return ut_malloc_trace(size, 0, UT_MEM_FLAG_NOCHECK | flags, file, line, "DEVM-ALLOC");
}
EXPORT_SYMBOL_GPL(__devm_kmalloc);


/**
 * devm_kfree - Resource-managed kfree
 * @dev: Device this memory belongs to
 * @p: Memory to free
 *
 * Free memory allocated with devm_kmalloc().
 */
void devm_kfree(struct device *dev, void *p)
{
	ut_free(p);
}
EXPORT_SYMBOL_GPL(devm_kfree);

