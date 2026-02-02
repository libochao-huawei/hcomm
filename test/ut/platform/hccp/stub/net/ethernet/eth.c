#include "stdio.h"

#include <linux/module.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/errno.h>

void ether_setup(struct net_device *dev)
{}

/**
 * alloc_etherdev_mqs - Allocates and sets up an Ethernet device
 * @sizeof_priv: Size of additional driver-private structure to be allocated
 *	for this Ethernet device
 * @txqs: The number of TX queues this device has.
 * @rxqs: The number of RX queues this device has.
 *
 * Fill in the fields of the device structure with Ethernet-generic
 * values. Basically does everything except registering the device.
 *
 * Constructs a new net device, complete with a private data area of
 * size (sizeof_priv).  A 32-byte (not bit) alignment is enforced for
 * this private data area.
 */

struct net_device *alloc_etherdev_mqs(int sizeof_priv, unsigned int txqs,
				      unsigned int rxqs)
{
	return alloc_netdev_mqs(sizeof_priv, "eth%d", NET_NAME_UNKNOWN,
				ether_setup, txqs, rxqs);
}
EXPORT_SYMBOL(alloc_etherdev_mqs);

/**
 * eth_type_trans - determine the packet's protocol ID.
 * @skb: received socket data
 * @dev: receiving network device
 *
 * The rule here is that we
 * assume 802.3 if the type field is short enough to be a length.
 * This is normal practice and works for any 'now in use' protocol.
 */
__be16 eth_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	return 0;
}
EXPORT_SYMBOL(eth_type_trans);