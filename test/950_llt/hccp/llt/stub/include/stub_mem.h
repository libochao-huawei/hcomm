#ifndef _STUB_MEM_H_
#define _STUB_MEM_H_

#include <linux/gfp.h>

extern UT_CNT_RANGE_DECLARE(kmalloc, fail);
extern UT_CNT_RANGE_DECLARE(kfree, none);
extern UT_CNT_RANGE_DECLARE(devm_kmalloc, fail);

extern UT_CNT_RANGE_DECLARE(put_task_struct, dead);

/* define page_alloc.c */
extern UT_MAP_DECLARE(page_to_nid, proc);

extern UT_MAP_DECLARE(get_free_pages, proc);

extern UT_MAP_DECLARE(get_pid_task, proc);
extern UT_MAP_DECLARE(get_task_mm, proc);
extern  UT_MAP_DECLARE(remap_pfn_range, proc);

int stub_page_swap_error(void);
int stub_page_common_error(void);


#endif
