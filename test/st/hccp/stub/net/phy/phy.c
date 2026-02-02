#include "stdio.h"

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>

int phy_ethtool_gset(struct phy_device *phydev, struct ethtool_cmd *cmd)
{
	return 0;
}
EXPORT_SYMBOL(phy_ethtool_gset);

int phy_ethtool_sset(struct phy_device *phydev, struct ethtool_cmd *cmd)
{
        return 0;
}
EXPORT_SYMBOL(phy_ethtool_sset);

int phy_ethtool_ksettings_set(struct phy_device *phydev,
			      const struct ethtool_link_ksettings *cmd)
{
	return 0;				  
}
EXPORT_SYMBOL(phy_ethtool_ksettings_set);

void phy_ethtool_ksettings_get(struct phy_device *phydev,
			       struct ethtool_link_ksettings *cmd)
{
	
}
EXPORT_SYMBOL(phy_ethtool_ksettings_get);


int phy_start_aneg(struct phy_device *phydev)
{
	return 0;
}
EXPORT_SYMBOL(phy_start_aneg);
/**
 * phy_start - start or restart a PHY device
 * @phydev: target phy_device struct
 *
 * Description: Indicates the attached device's readiness to
 *   handle PHY-related work.  Used during startup to start the
 *   PHY, and after a call to phy_stop() to resume operation.
 *   Also used to indicate the MDIO bus has cleared an error
 *   condition.
 */
void phy_start(struct phy_device *phydev)
{
}
EXPORT_SYMBOL(phy_start);

/**
 * phy_stop - Bring down the PHY link, and stop checking the status
 * @phydev: target phy_device struct
 */
void phy_stop(struct phy_device *phydev)
{

}
EXPORT_SYMBOL(phy_stop);
