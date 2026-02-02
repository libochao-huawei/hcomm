#include <asm/pgtable_types.h>


/*
 * Tables translating between page_cache_type_t and pte encoding.
 *
 * The default values are defined statically as minimal supported mode;
 * WC and WT fall back to UC-.  pat_init() updates these values to support
 * more cache modes, WC and WT, when it is safe to do so.  See pat_init()
 * for the details.  Note, __early_ioremap() used during early boot-time
 * takes pgprot_t (pte encoding) and does not use these tables.
 *
 *   Index into __cachemode2pte_tbl[] is the cachemode.
 *
 *   Index into __pte2cachemode_tbl[] are the caching attribute bits of the pte
 *   (_PAGE_PWT, _PAGE_PCD, _PAGE_PAT) at index bit positions 0, 1, 2.
 */
uint16_t __cachemode2pte_tbl[_PAGE_CACHE_MODE_NUM] = {
	[_PAGE_CACHE_MODE_WB      ]	= 0         | 0        ,
	[_PAGE_CACHE_MODE_WC      ]	= 0         | _PAGE_PCD,
	[_PAGE_CACHE_MODE_UC_MINUS]	= 0         | _PAGE_PCD,
	[_PAGE_CACHE_MODE_UC      ]	= _PAGE_PWT | _PAGE_PCD,
	[_PAGE_CACHE_MODE_WT      ]	= 0         | _PAGE_PCD,
	[_PAGE_CACHE_MODE_WP      ]	= 0         | _PAGE_PCD,
};
EXPORT_SYMBOL(__cachemode2pte_tbl);

