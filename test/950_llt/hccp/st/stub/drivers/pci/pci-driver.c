#include "stdio.h"

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>

struct pci_driver *tc_pci_driver = NULL;
int tc_pci_dev_probe_dev_id = 0xA220;

DECLARE_RWSEM(pci_bus_sem);
LIST_HEAD(pci_root_buses);

/**
 * __pci_register_driver - register a new pci driver
 * @drv: the driver structure to register
 * @owner: owner module of drv
 * @mod_name: module name string
 *
 * Adds the driver structure to the list of registered drivers.
 * Returns a negative value on error, otherwise 0.
 * If no error occurred, the driver remains registered even if
 * no device was claimed during registration.
 */
int __pci_register_driver(struct pci_driver *drv, struct module *owner,
			  const char *mod_name)
{
	/* initialize common driver fields */

	tc_pci_driver = drv;

	/* initialize common driver fields */
	drv->driver.name = drv->name;
	drv->driver.owner = owner;
	drv->driver.mod_name = mod_name;
	//drv->driver.groups = drv->groups;

	/* register with core */
	return 0;
}
EXPORT_SYMBOL(__pci_register_driver);


/**
 * pci_unregister_driver - unregister a pci driver
 * @drv: the driver structure to unregister
 *
 * Deletes the driver structure from the list of registered PCI drivers,
 * gives it a chance to clean up by calling its remove() function for
 * each device it was responsible for, and marks those devices as
 * driverless.
 */

void pci_unregister_driver(struct pci_driver *drv)
{
	tc_pci_driver = NULL;
}
EXPORT_SYMBOL(pci_unregister_driver);

/**
 * pci_match_id - See if a pci device matches a given pci_id table
 * @ids: array of PCI device id structures to search in
 * @dev: the PCI device structure to match against.
 *
 * Used by a driver to check whether a PCI device present in the
 * system is in its list of supported devices.  Returns the matching
 * pci_device_id structure or %NULL if there is no match.
 *
 * Deprecated, don't use this as it will not catch any dynamic ids
 * that a driver might want to check for.
 */
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
					 struct pci_dev *dev)
{
	int search_nr = 0;
	if (ids) {
		while (ids->vendor || ids->subvendor || ids->class_mask) {
			if (ids->device == tc_pci_dev_probe_dev_id) {
				dev->device = ids->device;
				return ids;
			}
			ids++;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(pci_match_id);

/**
 * put_device - decrement reference count.
 * @dev: device in question.
 */
void put_device(struct device *dev)
{
	/* might_sleep(); */
}
EXPORT_SYMBOL_GPL(put_device);

/**
 * pci_dev_put - release a use of the pci device structure
 * @dev: device that's been disconnected
 *
 * Must be called when a user of a device is finished with it.  When the last
 * user of the device calls this function, the memory of the device is freed.
 */
void pci_dev_put(struct pci_dev *dev)
{
	if (dev)
		put_device(&dev->dev);
}
EXPORT_SYMBOL(pci_dev_put);

/**
 * pci_find_next_bus - begin or continue searching for a PCI bus
 * @from: Previous PCI bus found, or %NULL for new search.
 *
 * Iterates through the list of known PCI buses.  A new search is
 * initiated by passing %NULL as the @from argument.  Otherwise if
 * @from is not %NULL, searches continue from next device on the
 * global list.
 */
struct pci_bus *pci_find_next_bus(const struct pci_bus *from)
{
	struct list_head *n;
	struct pci_bus *b = NULL;

	WARN_ON(in_interrupt());
	n = from ? from->node.next : pci_root_buses.next;
	if (n != &pci_root_buses)
		b = list_entry(n, struct pci_bus, node);
	return b;
}
EXPORT_SYMBOL(pci_find_next_bus);

static struct pci_bus *pci_do_find_bus(struct pci_bus *bus, unsigned char busnr)
{
	struct pci_bus *child;
	struct pci_bus *tmp;

	if (bus->number == busnr)
		return bus;

	list_for_each_entry(tmp, &bus->children, node) {
		child = pci_do_find_bus(tmp, busnr);
		if (child)
			return child;
	}
	return NULL;
}

struct pci_bus tc_pci_bus;

/**
 * pci_find_bus - locate PCI bus from a given domain and bus number
 * @domain: number of PCI domain to search
 * @busnr: number of desired PCI bus
 *
 * Given a PCI bus number and domain number, the desired PCI bus is located
 * in the global list of PCI buses.  If the bus is found, a pointer to its
 * data structure is returned.  If no bus is found, %NULL is returned.
 */
struct pci_bus *pci_find_bus(int domain, int busnr)
{
	/*
	struct pci_bus *bus = NULL;
	struct pci_bus *tmp_bus;

	while ((bus = pci_find_next_bus(bus)) != NULL)  {
		if (pci_domain_nr(bus) != domain)
			continue;
		tmp_bus = pci_do_find_bus(bus, busnr);
		if (tmp_bus)
			return tmp_bus;
	}
	*/
	return &tc_pci_bus;
}
EXPORT_SYMBOL(pci_find_bus);

/**
 * pci_get_slot - locate PCI device for a given PCI slot
 * @bus: PCI bus on which desired PCI device resides
 * @devfn: encodes number of PCI slot in which the desired PCI
 * device resides and the logical device number within that slot
 * in case of multi-function devices.
 *
 * Given a PCI bus and slot/function number, the desired PCI device
 * is located in the list of PCI devices.
 * If the device is found, its reference count is increased and this
 * function returns a pointer to its data structure.  The caller must
 * decrement the reference count by calling pci_dev_put().
 * If no device is found, %NULL is returned.
 */
struct pci_dev tc_pci_dev;
struct pci_dev *pci_get_slot(struct pci_bus *bus, unsigned int devfn)
{
/*
	struct pci_dev *dev;

	WARN_ON(in_interrupt());

	list_for_each_entry(dev, &bus->devices, bus_list) {
		if (dev->devfn == devfn)
			goto out;
	}

	dev = NULL;
 out:
 */
 	pci_resource_start((&tc_pci_dev), (4)) = 0;
	pci_resource_end((&tc_pci_dev), (4)) = 0xFFFFFF;
	pci_resource_start((&tc_pci_dev), (5)) = 0xFFFFFF;
	pci_resource_end((&tc_pci_dev), (5)) = 0x1FFFFFF;
	return &tc_pci_dev;
}
EXPORT_SYMBOL(pci_get_slot);

struct pci_dev *pci_get_domain_bus_and_slot(int domain, unsigned int bus,
                                            unsigned int devfn)
{
	return pci_get_slot(pci_find_bus(0, bus), devfn);
}
EXPORT_SYMBOL(pci_get_domain_bus_and_slot);

struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device,
			       struct pci_dev *from)
{
	//return &tc_stub_pf2_pci_dev;
	return pci_get_slot(pci_find_bus(0, 0), 0);
	//return pci_get_subsys(vendor, device, PCI_ANY_ID, PCI_ANY_ID, from);
}
EXPORT_SYMBOL(pci_get_device);

static int pci_call_probe(struct pci_driver *drv, struct pci_dev *dev,
			  const struct pci_device_id *id)
{
	int rc;

	//dev->is_probed = 1;
	dev->revision = 0x21;
	dev->driver = drv;
	do {
		rc = drv->probe(dev, id);
		if (!rc)
			break;
		if (rc < 0) {
			dev->driver = NULL;
			break;
		}
	} while(0);

	//dev->is_probed = 0;

	return rc;
}


/**
 * __pci_device_probe - check if a driver wants to claim a specific PCI device
 * @drv: driver to call to check if it wants the PCI device
 * @pci_dev: PCI device being probed
 *
 * returns 0 on success, else error.
 * side-effect: pci_dev->driver is set to drv when drv claims pci_dev.
 */
static int __pci_device_probe(struct pci_driver *drv, struct pci_dev *pci_dev)
{
	const struct pci_device_id *id;
	int error = 0;

	if (!pci_dev->driver && drv->probe) {
		error = -ENODEV;

		id = pci_match_id(drv->id_table, pci_dev);
		if (id)
			error = pci_call_probe(drv, pci_dev, id);
	}

	return error;
}

int stub_pci_device_probe(struct pci_dev *pci_dev)
{
	if (tc_pci_driver && pci_dev)
		return __pci_device_probe(tc_pci_driver, pci_dev);

	return 0;
}

int stub_pci_device_remove(struct pci_dev *pci_dev)
{
	struct pci_driver *drv = pci_dev->driver;

	if (drv) {
		if (drv->remove) {
			drv->remove(pci_dev);
		}
		pci_dev->driver = NULL;
	}
	return 0;
}

struct bus_type pci_bus_type = {
	.name		= "pci",
//	.match		= pci_bus_match,
//	.uevent		= pci_uevent,
//	.probe		= pci_device_probe,
//	.remove		= pci_device_remove,
//	.shutdown	= pci_device_shutdown,
//	.dev_groups	= pci_dev_groups,
//	.bus_groups	= pci_bus_groups,
//	.drv_groups	= pci_drv_groups,
//	.pm		= PCI_PM_OPS_PTR,
};
EXPORT_SYMBOL(pci_bus_type);

