#include "ut_lib.h"

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/types.h>

UT_CNT_RANGE_DEFINE(dma_map_single_attrs, fail);
dma_addr_t __dma_map_single_attrs(struct device *dev, void *ptr,
					      size_t size,
					      enum dma_data_direction dir,
					      unsigned long attrs, const char *file, int line)
{
	if (UT_CNT_RANGE_CHECK(dma_map_single_attrs, fail, 1))
		return NULL;

	return (dma_addr_t)ut_malloc_trace(size, 0, 0, 
			file, line, "DMA-SINGLE-MAP:dir=%x,attrs=%x", dir, attrs);
}

void dma_unmap_single_attrs(struct device *dev, dma_addr_t addr,
					  size_t size,
					  enum dma_data_direction dir,
					  struct dma_attrs *attrs)
{
	ut_free((void *)addr);
}

UT_CNT_RANGE_DEFINE(dma_map_page_attrs, fail);
dma_addr_t __dma_map_page_attrs(struct device *dev,
					    struct page *page,
					    size_t offset, size_t size,
					    enum dma_data_direction dir,
					    unsigned long attrs, const char *file, int line)

{
	if (UT_CNT_RANGE_CHECK(dma_map_page_attrs, fail, 1))
		return 0;

	return (dma_addr_t)ut_malloc_trace(size, 0, 0, 
			file, line, "DMA-PAGE-MAP:ofs=%x,dir=%x,attrs=%x", offset, dir, attrs);
}


void dma_unmap_page_attrs(struct device *dev,
					dma_addr_t addr, size_t size,
					enum dma_data_direction dir,
					unsigned long attrs)
{
	ut_free((void *)addr);
}

int dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return (0 == dma_addr);
}

int dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
	return 0;
}

UT_CNT_RANGE_DEFINE(dma_alloc_coherent, fail);
void *dma_alloc_coherent(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t flag)
{
	if (UT_CNT_RANGE_CHECK(dma_alloc_coherent, fail, 1))
		return NULL;

	return __kmalloc(size, flag | __GFP_ZERO, __FILE__, __LINE__);
}

UT_CNT_RANGE_DEFINE(dma_free_coherent, none);
void dma_free_coherent(struct device *dev, size_t size,
		void *cpu_addr, dma_addr_t dma_handle)
{
	if (UT_CNT_RANGE_CHECK(dma_free_coherent, none, 1))
		return;
	kfree(cpu_addr);
}

struct dma_pool {		/* the pool */
	struct list_head page_list;
	spinlock_t lock;
	size_t size;
	struct device *dev;
	size_t allocation;
	size_t boundary;
	char name[32];
	struct list_head pools;
};

UT_CNT_RANGE_DEFINE(dma_pool_create, fail);
struct dma_pool *dma_pool_create(const char *name, struct device *dev,
			size_t size, size_t align, size_t allocation)
{
	struct dma_pool * ptr;

	if (UT_CNT_RANGE_CHECK(dma_pool_create, fail, 1))
		return NULL;

	ptr = (struct dma_pool *)ut_malloc_trace(sizeof(struct dma_pool), 0, 0,
			__FILE__, __LINE__, "dma_pool_create:name=%s,size=%x", name, size);

	if (ptr)
		ptr->allocation = allocation;
	return ptr;
}

UT_CNT_RANGE_DEFINE(dma_pool_alloc, fail);
void *dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags,
		     dma_addr_t *handle)
{
	if (UT_CNT_RANGE_CHECK(dma_pool_alloc, fail, 1))
		return NULL;

        void *ptr = (void *)ut_malloc_trace(128, 0, 0,
			__FILE__, __LINE__, "dma_pool_alloc");

	return ptr;
}

void dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t addr)
{ 
	kfree(vaddr); 
};
