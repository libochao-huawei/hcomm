#include "ut_lib.h"

#include <linux/pci.h>

/**
 * devm_ioremap_resource() - check, request region, and ioremap resource
 * @dev: generic device to handle the resource for
 * @res: resource to be handled
 *
 * Checks that a resource is a valid memory region, requests the memory
 * region and ioremaps it. All operations are managed and will be undone
 * on driver detach.
 *
 * Returns a pointer to the remapped memory or an ERR_PTR() encoded error code
 * on failure. Usage example:
 *
 *	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
 *	base = devm_ioremap_resource(&pdev->dev, res);
 *	if (IS_ERR(base))
 *		return PTR_ERR(base);
 */
void __iomem *__devm_ioremap_resource(struct device *dev, struct resource *res,
		const char *file, int line)
{
	resource_size_t len = resource_size(res);

	return (void __iomem *)ut_malloc_trace(len, 0, UT_MEM_FLAG_NOCHECK, file, line,
			"DEVM-IOREMAP");
}
EXPORT_SYMBOL(__devm_ioremap_resource);

/**
 * devm_ioremap - Managed ioremap()
 * @dev: Generic device to remap IO address for
 * @offset: Resource address to map
 * @size: Size of map
 *
 * Managed ioremap().  Map is automatically unmapped on driver detach.
 */
void __iomem *devm_ioremap(struct device *dev, resource_size_t offset,
			   resource_size_t size)
{
	return (void __iomem *)ut_malloc_trace(size, 0, UT_MEM_FLAG_NOCHECK, __FILE__, __LINE__,
			"DEVM-IOREMAP");
}
EXPORT_SYMBOL(devm_ioremap);

void devm_iounmap(struct device *dev, void __iomem *addr)
{
	ut_free(addr);
}
EXPORT_SYMBOL(devm_iounmap);

volatile void *bar_addr = NULL;
#define BAR_LEN 0x100000
/**
 * pcim_iomap - Managed pcim_iomap()
 * @pdev: PCI device to iomap for
 * @bar: BAR to iomap
 * @maxlen: Maximum length of iomap
 *
 * Managed pci_iomap().  Map is automatically unmapped on driver
 * detach.
 */
void __iomem *__pcim_iomap(struct pci_dev *pdev, int bar, unsigned long maxlen,
		const char *file, int line)
{
	resource_size_t len = pci_resource_len(pdev, bar);

	/*return (void __iomem *)ut_malloc_trace(len, 0, UT_MEM_FLAG_NOCHECK, file, line,
			"PCIM-IOMAP:bar[%d]", bar);*/

	if (bar_addr)
		memset(bar_addr, 0 , BAR_LEN);

	return bar_addr;
}

EXPORT_SYMBOL(__pcim_iomap);

/**
 * pcim_iounmap - Managed pci_iounmap()
 * @pdev: PCI device to iounmap for
 * @addr: Address to unmap
 *
 * Managed pci_iounmap().  @addr must have been mapped using pcim_iomap().
 */
void pcim_iounmap(struct pci_dev *pdev, void __iomem *addr)
{
	//ut_free(addr);
	//free(bar_addr);
	memset(bar_addr, 0 , BAR_LEN);
	//bar_addr = NULL;
	//printf("pcim_iounmap \n");
}
EXPORT_SYMBOL(pcim_iounmap);


#include <linux/module.h>
static int __init stub_pci_bar_init_module(void)
{
	bar_addr = malloc(BAR_LEN);
	memset(bar_addr, 0 , BAR_LEN);
}

module_init(stub_pci_bar_init_module);

static void __exit stub_pci_bar_exit_module(void)
{
	free(bar_addr);
	bar_addr = NULL;
}
module_exit(stub_pci_bar_exit_module);


