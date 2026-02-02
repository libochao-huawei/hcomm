#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/mm_inline.h>
#include <linux/percpu_counter.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/backing-dev.h>
#include <linux/memcontrol.h>
#include <linux/gfp.h>
#include <linux/uio.h>
#include "ut_lib.h"

static UT_MAP_DEFINE(get_page, trace);

int stub_page_swap_error(void)
{
	return UT_MAP_CLR(get_page, trace);
}

void _get_page(struct page *page, const char *file, int line)
{
	void *ptr = ut_malloc_trace(sizeof(*page), 0, 0, file, line, "get_page");

	UT_MAP_INSERT(get_page, trace, page, ptr);
}

void _put_page(struct page *page, const char *file, int line)
{
	void *ptr = UT_MAP_ERASE(get_page, trace, page);
	if (ptr)
		ut_free_trace(ptr, 0, file, line, NULL);
}
EXPORT_SYMBOL(_put_page);

