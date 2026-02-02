#include "stdio.h"

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>

int genphy_restart_aneg(struct phy_device *phydev)
{
	/* Don't isolate the PHY if we're negotiating */
	return 0;
}
EXPORT_SYMBOL(genphy_restart_aneg);



int tc_adjust_link;
int genphy_read_status(struct phy_device *phydev)
{
	return 0;
}

/**
 * phy_connect_direct - connect an ethernet device to a specific phy_device
 * @dev: the network device to connect
 * @phydev: the pointer to the phy device
 * @handler: callback function for state change notifications
 * @interface: PHY device's interface
 */
int phy_connect_direct(struct net_device *dev, struct phy_device *phydev,
		       void (*handler)(struct net_device *),
		       phy_interface_t interface)
{
	phydev->adjust_link = handler;
	tc_adjust_link = 1;
	return 0;
}
EXPORT_SYMBOL(phy_connect_direct);

/**
 * phy_disconnect - disable interrupts, stop state machine, and detach a PHY
 *		    device
 * @phydev: target phy_device struct
 */
void phy_disconnect(struct phy_device *phydev)
{
	phydev->adjust_link = NULL;
	tc_adjust_link = 0;

}
EXPORT_SYMBOL(phy_disconnect);