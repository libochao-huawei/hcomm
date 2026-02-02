#ifndef _STUB_PCI_H_
#define _STUB_PCI_H_

#include <linux/pci.h>

extern int tc_pci_irq_alloc_cnt;
extern int tc_pci_irq_free_cnt;

extern int tc_pci_sriov_totalvfs_cnt;
extern int tc_pci_irq_vector;

extern int tc_pci_dev_probe_dev_id;

int stub_pci_device_probe(struct pci_dev *pci_dev);
int stub_pci_device_remove(struct pci_dev *pci_dev);

int stub_pci_common_error(void);
int stub_pci_irq_error(void);

extern UT_RETURN_STUB_DECLARE(__pci_iomap);

#endif
