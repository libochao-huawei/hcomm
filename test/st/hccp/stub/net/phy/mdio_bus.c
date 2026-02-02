#include <linux/phy.h>


/**
 * mdiobus_read - Convenience function for reading a given MII mgmt register
 * @bus: the mii_bus struct
 * @addr: the phy address
 * @regnum: register number to read
 *
 * NOTE: MUST NOT be called from interrupt context,
 * because the bus read/write functions may wait for an interrupt
 * to conclude the operation.
 */
int mdiobus_read(struct mii_bus *bus, int addr, u32 regnum)
{
 	return bus->read(bus, addr, regnum);
}
EXPORT_SYMBOL(mdiobus_read);

int mdiobus_write(struct mii_bus *bus, int addr, unsigned int regnum, unsigned short val)
{
	return bus->write(bus, addr, regnum, val);
}

void mdiobus_unregister(struct mii_bus *bus)
{
	bus->state = MDIOBUS_UNREGISTERED;
}

struct mii_bus *devm_mdiobus_alloc_size(struct device *dev, int sizeof_priv)
{
	return devm_kmalloc(dev, sizeof(struct mii_bus), 0);
}

void devm_mdiobus_free(struct device *dev, struct mii_bus *bus)
{
	return;
}

int __mdiobus_register(struct mii_bus *bus, struct module *owner)
{
	int i;
	
	for (i = 0; i < PHY_MAX_ADDR; i++) {
		if ((bus->phy_mask & (1 << i)) == 0) {
			struct phy_device *phydev;
			phydev = devm_kmalloc(&bus->dev, sizeof(struct phy_device), 0);
			phydev->bus = bus;
			phydev->bus->phy_map[i] = phydev;
			//phydev->flags |= MDIO_DEVICE_FLAG_PHY;
                }
        }
	
	bus->state = MDIOBUS_REGISTERED;
	return 0;
}

struct phy_device *mdiobus_get_phy(struct mii_bus *bus, int addr)
{
	struct phy_device *phydev = bus->phy_map[addr];

	if (!phydev)
		return NULL;

	return phydev;
}

