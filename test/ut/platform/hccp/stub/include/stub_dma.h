#ifndef _STUB_DMA_H_
#define _STUB_DMA_H_


extern UT_CNT_RANGE_DECLARE(dma_map_single_attrs, fail);
extern UT_CNT_RANGE_DECLARE(dma_map_page_attrs, fail);
extern UT_CNT_RANGE_DECLARE(dma_free_coherent, none);
extern UT_CNT_RANGE_DECLARE(dma_alloc_coherent, fail);
extern UT_CNT_RANGE_DECLARE(dma_pool_create, fail);
extern UT_CNT_RANGE_DECLARE(dma_pool_alloc, fail);


#endif
