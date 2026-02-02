#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/jiffies.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/kasan.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/pagevec.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/oom.h>
#include <linux/notifier.h>
#include <linux/topology.h>
#include <linux/sysctl.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/memory_hotplug.h>
#include <linux/nodemask.h>
#include <linux/vmalloc.h>
#include <linux/vmstat.h>
#include <linux/mempolicy.h>
#include "ut_lib.h"

DEFINE_PER_CPU(int, numa_node);
EXPORT_PER_CPU_SYMBOL(numa_node);

void mmput(struct mm_struct *ms)
{

}

UT_CNT_RANGE_DEFINE(put_task_struct, dead);
void put_task_struct(struct task_struct *t)
{
	if (UT_CNT_RANGE_CHECK(put_task_struct, dead, 1)) {
		if (t)
			t->state = TASK_DEAD;
	}
}

UT_MAP_DEFINE(get_task_mm, proc);
struct mm_struct *get_task_mm(struct task_struct *task)
{
	return (struct mm_struct *)UT_MAP_FIND(get_task_mm, proc, task);
}

UT_MAP_DEFINE(get_pid_task, proc);
struct task_struct *get_pid_task(struct pid *pid, enum pid_type pt)
{
	return (struct mm_struct *)UT_MAP_FIND(get_pid_task, proc, pt);
}

UT_MAP_DEFINE(remap_pfn_range, proc);
int remap_pfn_range(struct vm_area_struct *v, unsigned long addr,
			unsigned long pfn, unsigned long size, pgprot_t p)
{
	if (UT_MAP_EXIST(remap_pfn_range, proc, addr))
		return (int)UT_MAP_FIND(remap_pfn_range, proc, addr);
	else
		return -ENOMEM;
}

void set_numa_node(int node)
{
	int cpu = 0;

	//per_cpu(numa_node, cpu) = node;
}

int numa_node_id(void)
{
	return 0;//raw_cpu_read(numa_node);
}


int numa_mem_id(void)
{
	return 1;
}

 /**
  * Determine the allocation order of a particular sized block of memory.  This
  * is on a logarithmic scale, where:
  *
  *  0 -> 2^0 * PAGE_SIZE and below
  *  1 -> 2^1 * PAGE_SIZE to 2^0 * PAGE_SIZE + 1
  *  2 -> 2^2 * PAGE_SIZE to 2^1 * PAGE_SIZE + 1
  *  3 -> 2^3 * PAGE_SIZE to 2^2 * PAGE_SIZE + 1
  *  4 -> 2^4 * PAGE_SIZE to 2^3 * PAGE_SIZE + 1
  *  ...
  *
  * The order returned is used to find the smallest allocation granule required
  * to hold an object of the specified size.
  */

/*
 * This is the 'heart' of the zoned buddy allocator.
 */
struct page *
__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order, struct zonelist *zonelist,
							nodemask_t *nodemask)
{
	void *p = NULL;
	int flags = UT_MEM_FLAG_NOCHECK;
	int total = 1;

	while(order-- > 0)
		total *= 2;

	if (__GFP_ZERO & gfp_mask)
		flags |= UT_MEM_FLAG_ZERO;

	p = ut_malloc_trace(total << PAGE_SHIFT, sizeof(struct page), flags,
				__FILE__, __LINE__, "PAGE-ALLOC:order[%d]", order);

	if (p) {
		struct page *pg = (struct page *)ut_mem_oob_data(p);

		pg->virtual = p;

		return pg;
	}

	return NULL;
}
EXPORT_SYMBOL(__alloc_pages_nodemask);

UT_MAP_DEFINE(get_free_pages, proc);
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	return (unsigned long)UT_MAP_FIND(get_free_pages, proc, order);
}

void *page_address(const struct page *page)
{
	return page->virtual;
}

UT_MAP_DEFINE(page_to_nid, proc);
int page_to_nid(const struct page *page)
{
	return (int)UT_MAP_FIND(page_to_nid, proc, page);
}

int stub_page_common_error(void)
{
	UT_MAP_CLR(page_to_nid, proc);
	UT_MAP_CLR(get_task_mm, proc);
	UT_MAP_CLR(get_pid_task, proc);
	UT_MAP_CLR(remap_pfn_range, proc);
	UT_MAP_CLR(get_free_pages, proc);
	return 0;
}

void zap_vma_ptes(struct vm_area_struct *vma, unsigned long address,
		unsigned long size)
{

}
EXPORT_SYMBOL_GPL(zap_vma_ptes);

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
        return __get_free_pages(gfp_mask | __GFP_ZERO, 0);
}
EXPORT_SYMBOL(get_zeroed_page);
