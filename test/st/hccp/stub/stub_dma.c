#include "ut_lib.h"

#include <linux/dma-mapping.h>
#include <linux/slab.h>

UT_CNT_RANGE_DEFINE(__dma_map_single_attrs, fail);
dma_addr_t __dma_map_single_attrs(struct device *dev, void *ptr,
					      size_t size,
					      enum dma_data_direction dir,
					      struct dma_attrs * attrs, const char *file, int line)
{
	//if (UT_CNT_RANGE_CHECK(__dma_map_single_attrs, fail, 1))
	//	return NULL;

	return (dma_addr_t)ptr;/*(dma_addr_t)ut_malloc_trace(size, 0, 0, 
			file, line, "DMA-SINGLE-MAP:dir=%x,attrs=%x", dir, attrs);*/
}

void dma_unmap_single_attrs(struct device *dev, dma_addr_t addr,
					  size_t size,
					  enum dma_data_direction dir,
					  struct dma_attrs *attrs)
{
	//ut_free((void *)addr);
}

UT_CNT_RANGE_DEFINE(__dma_map_page_attrs, fail);
dma_addr_t __dma_map_page_attrs(struct device *dev,
					    struct page *page,
					    size_t offset, size_t size,
					    enum dma_data_direction dir,
					    struct dma_attrs * attrs, const char *file, int line)

{

	return (dma_addr_t)ut_malloc_trace(size, 0, 0, 
			file, line, "DMA-PAGE-MAP:ofs=%x,dir=%x,attrs=%x", offset, dir, attrs);
}


void dma_unmap_page_attrs(struct device *dev,
					dma_addr_t addr, size_t size,
					enum dma_data_direction dir,
					struct dma_attrs * attrs)
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

#if 1
UT_CNT_RANGE_DEFINE(dma_alloc_coherent, fail);
void *dma_alloc_coherent(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t flag)
{
	if (UT_CNT_RANGE_CHECK(dma_alloc_coherent, fail, 1))
		return NULL;

	void *ret = __kmalloc(size, flag | __GFP_ZERO);
	*dma_handle = ret;
	//printf("dma_alloc_coherent:0x%p 0x%p\n", ret, *dma_handle);
	return ret;
}

void dma_free_coherent(struct device *dev, size_t size,
		void *cpu_addr, dma_addr_t dma_handle)
{
	//printf("dma_free_coherent:0x%p 0x%p\n", cpu_addr, dma_handle);
	kfree(cpu_addr);
}
#else
typedef struct dma_allign_map {
        int valid;
        void *alloc;
        void *allign;
}dma_map;

dma_map st_dma_map[1024] = {0};


void *dma_alloc_coherent(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t flag)
{
        int i,j;
        void *ret = __kmalloc(size + (1 << 12), flag | __GFP_ZERO);

        for (i = 0; i < 1024; i++) {
                if (st_dma_map[i].valid == 0)
                {
                      st_dma_map[i].valid = 1;
                      st_dma_map[i].alloc = ret;
                      break;
                }
        }

	if (i >= 1024)
		return ret;
        
        //page allign
        if ((u64)ret & ((1 << 12) - 1)) {
                for (j = 0; j < (1 << 12); j++) {
                        ret++;
                        if (!((u64)ret & ((1 << 12) - 1))) {
                                break;
                        }
                }
        }
        
	*dma_handle = ret;
	st_dma_map[i].allign = ret;

	return ret;
}

void dma_free_coherent(struct device *dev, size_t size,
		void *cpu_addr, dma_addr_t dma_handle)
{
        int i;
        for (i = 0; i < 1024; i++) {
                if (st_dma_map[i].valid == 1 && st_dma_map[i].allign == cpu_addr)
                {
                      st_dma_map[i].valid = 0;
                      cpu_addr = st_dma_map[i].alloc;
                      break;
                }
        }

	printf("dma_free_coherent:0x%p 0x%p\n", cpu_addr, dma_handle);
        kfree(cpu_addr);
}
#endif
void *dma_zalloc_coherent(struct device *dev, size_t size,
					dma_addr_t *dma_handle, gfp_t flag)
{
	void *ret = dma_alloc_coherent(dev, size, dma_handle,
				       flag | __GFP_ZERO);

	*dma_handle = ret;
	//printf("dma_alloc_coherent:0x%p 0x%p\n", *dma_handle, ret);
	return ret;
}

void *dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags,
                     dma_addr_t *handle)
{
	void *ret = __kmalloc(512, mem_flags | __GFP_ZERO);
	*handle = ret;
	
        return ret;
}

void dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t addr)
{ 
	kfree(vaddr); 
};
