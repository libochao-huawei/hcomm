#include "ut_lib.h"

#include <linux/pci.h>

/* Hide the details if this is a MMIO or PIO address space and just do what
 * you expect in the correct way. */
void __pci_iounmap(struct pci_dev *dev, void __iomem * addr,const char *file, int line)
{
	ut_free_trace(addr, 0, file, line, "pci_iounmap");
}
EXPORT_SYMBOL(__pci_iounmap);
