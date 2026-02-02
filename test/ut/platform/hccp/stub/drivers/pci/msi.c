#include "ut_lib.h"

#include <linux/pci.h>
#include <linux/msi.h>
struct msi_desc g_msi_entry;

int tc_pci_irq_vector = 4;

int pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
			  int minvec, int maxvec)
{
	return 12;
}
EXPORT_SYMBOL(pci_enable_msix_range);

int pci_enable_msi_range(struct pci_dev *dev, int minvec, int maxvec)
{
	return 12;
}
EXPORT_SYMBOL(pci_enable_msi_range);

void pci_disable_msi(struct pci_dev *dev) {};

void pci_disable_msix(struct pci_dev *dev) {};

/**
 * pci_alloc_irq_vectors_affinity - allocate multiple IRQs for a device
 * @dev:                PCI device to operate on
 * @min_vecs:           minimum number of vectors required (must be >= 1)
 * @max_vecs:           maximum (desired) number of vectors
 * @flags:              flags or quirks for the allocation
 * @affd:               optional description of the affinity requirements
 *
 * Allocate up to @max_vecs interrupt vectors for @dev, using MSI-X or MSI
 * vectors if available, and fall back to a single legacy vector
 * if neither is available.  Return the number of vectors allocated,
 * (which might be smaller than @max_vecs) if successful, or a negative
 * error code on error. If less than @min_vecs interrupt vectors are
 * available for @dev the function will fail with -ENOSPC.
 *
 * To get the Linux IRQ number used for a vector that can be passed to
 * request_irq() use the pci_irq_vector() helper.
 */
static UT_MAP_DEFINE(pci_alloc_irq_vectors, trace);

int stub_pci_irq_error(void)
{
	return UT_MAP_CLR(pci_alloc_irq_vectors, trace);
}

int __pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs,
				     unsigned int max_vecs, unsigned int flags,
				     const struct irq_affinity *affd, const char *file, int line)
{
	void *ptr = ut_malloc_trace(sizeof(*dev), 0, 0, file, line,
				    "pci_alloc_irq %s vecs[%d:%d]", dev->dev.init_name, min_vecs, max_vecs);

	UT_MAP_INSERT(pci_alloc_irq_vectors, trace, dev, ptr);

	return tc_pci_irq_vector + max_vecs;
}
EXPORT_SYMBOL(__pci_alloc_irq_vectors_affinity);


/**
 * pci_free_irq_vectors - free previously allocated IRQs for a device
 * @dev:                PCI device to operate on
 *
 * Undoes the allocations and enabling in pci_alloc_irq_vectors().
 */
void __pci_free_irq_vectors(struct pci_dev *dev, const char *file, int line)
{
	void *ptr = UT_MAP_ERASE(pci_alloc_irq_vectors, trace, dev);

	if (ptr) {
		ut_free_trace(ptr, 0, file, line, NULL);
	}
}
EXPORT_SYMBOL(__pci_free_irq_vectors);


/**
 * pci_irq_vector - return Linux IRQ number of a device vector
 * @dev: PCI device to operate on
 * @nr: device-relative interrupt vector index (0-based).
 */
int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	if (dev) {
		return dev->irq + nr;
	} else {
		return tc_pci_irq_vector + nr;
	}
}
EXPORT_SYMBOL(pci_irq_vector);
