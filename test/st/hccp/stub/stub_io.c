#include "ut_lib.h"

#include <asm/page.h> /* I/O is all done through memory accesses */
#include <linux/string.h> /* for memset() and memcpy() */
#include <linux/types.h>
#include <linux/module.h>

extern void *bar_addr;
extern int hclgepf_dev_status;
int hclge_dev_stat();
void imp_process();
#define HCLGE_NIC_CSQ_HEAD_REG		0x27014
#define ROCEE_TX_CMQ_HEAD_REG       0x07014
#define HCLGE_RAS_PF_OTHER_INT_STS_REG 0x20B00

void writel(u32 value, volatile void __iomem *addr)
{
	if (addr == 0x100090010) {
		return;
	}
	(*(volatile u32 *)addr) = value;
}

int hclgepf_dev_status = 0;
#define HCLGEPF_DEV_COR_RST 1
#define HCLGEPF_DEV_GLB_RST 2
#define HCLGEPF_DEV_IMP_RST 3
#define HCLGEPF_DEV_MBX 4
#define HCLGEPF_DEV_MSIX 5

#define HCLGE_VECTOR0_RX_CMDQ_INT_B	1
#define HCLGE_VECTOR0_GLOBALRESET_INT_B	5
#define HCLGE_VECTOR0_CORERESET_INT_B	6
#define HCLGE_VECTOR0_IMPRESET_INT_B	7

int hclgepf_dev_stat()
{
        switch(hclgepf_dev_status) {
        case  HCLGEPF_DEV_COR_RST:
                return BIT(HCLGE_VECTOR0_CORERESET_INT_B);
        case  HCLGEPF_DEV_GLB_RST:
                return BIT(HCLGE_VECTOR0_GLOBALRESET_INT_B);
        case  HCLGEPF_DEV_IMP_RST:
                return BIT(HCLGE_VECTOR0_IMPRESET_INT_B);
        case  HCLGEPF_DEV_MBX:
                return 0xF00;
        case  HCLGEPF_DEV_MSIX:
                return BIT(HCLGE_VECTOR0_RX_CMDQ_INT_B);
        default:
                break;
        }

        return 0;
}

u32 readl(const volatile void __iomem *addr)
{
	if ((addr == (bar_addr + HCLGE_NIC_CSQ_HEAD_REG)) || (addr == (bar_addr + ROCEE_TX_CMQ_HEAD_REG)))
		imp_process();
	if (hclgepf_dev_status) {
		return hclgepf_dev_stat();
	}
	if ((addr == bar_addr + HCLGE_RAS_PF_OTHER_INT_STS_REG))
		return 0x300FF00;
	if (addr == 0x100090010) {
		return 0;
	}
	return (*(volatile u32 *)addr);
}

UT_RETURN_STUB(readl, 0);

void __iomem *ioremap(phys_addr_t offset, size_t size)
{
	if (offset == 0x100090010) {
		return 0x100090010;
	}
	return offset;
}

void iounmap(void __iomem *kvaddr)
{
}

u16 __weak readw(const volatile void __iomem *addr)
{
	return 0;
}

void __weak writew(u16 value, volatile void __iomem *addr)
{
}

void __weak __raw_writew(u16 value, volatile void __iomem *addr)
{
	
}
