#include "ut_lib.h"

#include <asm/page.h> /* I/O is all done through memory accesses */
#include <linux/string.h> /* for memset() and memcpy() */
#include <linux/types.h>

u32 ras_enable_val = 0;
void writel(u32 value, volatile void __iomem *addr)
{
	if (addr == 0x100090010) {
		ras_enable_val = value;
	}
}
UT_CNT_RANGE_DEFINE(readl, proc);
UT_MAP_DEFINE(readl, proc);
u32 readl(const volatile void __iomem *addr)
{
	if (UT_MAP_EXIST(readl, proc, addr)){
		int ret = UT_CNT_RANGE_CHECK(readl, proc, 1);
		if (UT_MAP_EXIST_WITH_ID(readl, proc, addr, ret)){
			if (ret)
				return (u32)UT_MAP_FIND_WITH_ID(readl, proc, addr, ret);

		}
	}

	return 0;
}
UT_RETURN_STUB(readl, 0);

int stub_io_common_error(void)
{
	UT_MAP_CLR(readl, proc);
	return 0;
}

UT_MAP_DEFINE(__raw_writeq, proc);
void __raw_writeq(u64 value, volatile void __iomem *addr)
{
	return 0;
}

void __iomem *ioremap(phys_addr_t offset, size_t size)
{
	return offset;
}

void iounmap(void __iomem *kvaddr)
{
}

u16 __weak readw(volatile void __iomem *addr)
{
	return 0;
}

void __weak writew(u16 value, volatile void __iomem *addr)
{
}
void __weak __raw_writew(u16 value, volatile void __iomem *addr)
{
	
}
