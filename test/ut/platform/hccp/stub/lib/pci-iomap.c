
#include "ut_lib.h"

#include <linux/pci.h>

/**
 * pci_iomap - create a virtual mapping cookie for a PCI BAR
 * @dev: PCI device that owns the BAR
 * @bar: BAR number
 * @maxlen: length of the memory to map
 *
 * Using this function you will get a __iomem address to your device BAR.
 * You can access it using ioread*() and iowrite*(). These functions hide
 * the details if this is a MMIO or PIO address space and will just do what
 * you expect from them in the correct way.
 *
 * @maxlen specifies the maximum length to map. If you want to get access to
 * the complete BAR without checking for its length first, pass %0 here.
 * */
void __iomem *__pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen, const char *file, int line)
{
	resource_size_t len = pci_resource_len(dev, bar);
	
	return (void __iomem *)ut_malloc_trace(len, 0, 0, file, line,
			"PCI-IOMAP:bar[%d]", bar);
}
EXPORT_SYMBOL(__pci_iomap);
UT_RETURN_STUB(__pci_iomap, 0);

