#include "ut_lib.h"

#include <linux/pci.h>

#if 0
#define TRACE_PCI_REGIONS_ALLOC(dev_pci, file, line)    ut_printf("[PCI-REGIONS-ALLOC] %s %s:%i\n", \
		dev_pci->dev.init_name ? dev_pci->dev.init_name : "", file, line)
#define TRACE_PCI_REGIONS_FREE(dev_pci, file, line)     ut_printf("[PCI-REGIONS-FREE] %s %s:%i\n", \
		dev_pci->dev.init_name ? dev_pci->dev.init_name : "", file, line)
#else
#define TRACE_PCI_REGIONS_ALLOC(dev, file, line) do{}while(0)
#define TRACE_PCI_REGIONS_FREE(dev, file, line) do{}while(0)
#endif

static int stub_pci_regions_alloc_cnt = 0;
static int stub_pci_regions_free_cnt = 0;
static int stub_pci_dev_enable = 0;
static int stub_pci_dev_master = 0;

int stub_pci_common_error(void)
{
	int err = 0;

	if (stub_pci_regions_alloc_cnt != stub_pci_regions_free_cnt) {
		err++;
		ut_printf("[PCI-REGIONS-ERR] alloc = %d, free = %d\n",
			  stub_pci_regions_alloc_cnt, stub_pci_regions_free_cnt);
	}

	if (stub_pci_dev_enable) {
		ut_printf("[PCI-ENABLE-ERR] en = %d\n", stub_pci_dev_enable);
		err++;
	}

	if (stub_pci_dev_master) {
		err++;
		ut_printf("[PCI-MASTER-ERR] en = %d\n", stub_pci_dev_master);
	}

	stub_pci_regions_alloc_cnt = stub_pci_regions_free_cnt = 0;
	stub_pci_dev_enable = 0;
	stub_pci_dev_master = 0;

	return err;
}

/**
 * pci_request_selected_regions - Reserve selected PCI I/O and memory resources
 * @pdev: PCI device whose resources are to be reserved
 * @bars: Bitmask of BARs to be requested
 * @res_name: Name to be associated with resource
 */
int __pci_request_selected_regions(struct pci_dev *pdev, int bars,
				   const char *res_name, const char *file, int line)
{
	TRACE_PCI_REGIONS_ALLOC(pdev, file, line);

	stub_pci_regions_alloc_cnt++;

	return 0;
}
EXPORT_SYMBOL(__pci_request_selected_regions);

/**
 * pci_release_selected_regions - Release selected PCI I/O and memory resources
 * @pdev: PCI device whose resources were previously reserved
 * @bars: Bitmask of BARs to be released
 *
 * Release selected PCI I/O and memory resources previously reserved.
 * Call this function only after all use of the PCI regions has ceased.
 */
void __pci_release_selected_regions(struct pci_dev *pdev, int bars, const char *file, int line)
{
	TRACE_PCI_REGIONS_FREE(pdev, file, line);
	stub_pci_regions_free_cnt++;
}
EXPORT_SYMBOL(__pci_release_selected_regions);

/**
 * pci_disable_device - Disable PCI device after use
 * @dev: PCI device to be disabled
 *
 * Signal to the system that the PCI device is not in use by the system
 * anymore.  This only involves disabling PCI bus-mastering, if active.
 *
 * Note we don't actually disable the device until all callers of
 * pci_enable_device() have called pci_disable_device().
 */
void pci_disable_device(struct pci_dev *dev)
{
	stub_pci_dev_enable = 0;

	if (atomic_dec_return(&dev->enable_cnt) != 0)
		return;
}
EXPORT_SYMBOL(pci_disable_device);

/**
 * pci_enable_device - Initialize device before it's used by a driver.
 * @dev: PCI device to be initialized
 *
 *  Initialize device before it's used by a driver. Ask low-level code
 *  to enable I/O and memory. Wake up the device if it was suspended.
 *  Beware, this function can fail.
 *
 *  Note we don't actually enable the device many times if we call
 *  this function repeatedly (we just increment the count).
 */
int pci_enable_device(struct pci_dev *dev)
{
	stub_pci_dev_enable = 1;
	if (atomic_inc_return(&dev->enable_cnt) > 1)
		return 0;		/* already enabled */
	
	return 0;
}

/**
 * pci_select_bars - Make BAR mask from the type of resource
 * @dev: the PCI device for which BAR mask is made
 * @flags: resource type mask to be selected
 *
 * This helper routine makes bar mask from the type of resource.
 */
int pci_select_bars(struct pci_dev *dev, unsigned long flags)
{
	return 0;
}
EXPORT_SYMBOL(pci_select_bars);


/**
 * pci_set_master - enables bus-mastering for device dev
 * @dev: the PCI device to enable
 *
 * Enables bus-mastering on the device and calls pcibios_set_master()
 * to do the needed arch specific settings.
 */
void pci_set_master(struct pci_dev *dev)
{
	stub_pci_dev_master = 1;
}
EXPORT_SYMBOL(pci_set_master);

/**
 * pci_clear_master - disables bus-mastering for device dev
 * @dev: the PCI device to disable
 */
void pci_clear_master(struct pci_dev *dev)
{
	stub_pci_dev_master = 0;
}
EXPORT_SYMBOL(pci_clear_master);



