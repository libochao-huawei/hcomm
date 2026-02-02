#include "stdio.h"
#include <stdarg.h>

#include <linux/kern_levels.h>
#include <linux/netdevice.h>
#include <linux/netfilter_ingress.h>
#include <linux/vmalloc.h>
#include <ut_lib.h>

#define define_netdev_printk_level(func, level)			\
void func(const struct net_device *dev, const char *fmt, ...)	\
{								\
	va_list args;						\
								\
	va_start(args, fmt);					\
	vprintf(fmt, args);		\
								\
	va_end(args);						\
}

struct netdev_hw_addr __ha;

define_netdev_printk_level(netdev_emerg, KERN_EMERG);
define_netdev_printk_level(netdev_alert, KERN_ALERT);
define_netdev_printk_level(netdev_crit, KERN_CRIT);
define_netdev_printk_level(netdev_err, KERN_ERR);
define_netdev_printk_level(netdev_warn, KERN_WARNING);
define_netdev_printk_level(netdev_notice, KERN_NOTICE);
define_netdev_printk_level(netdev_info, KERN_INFO);

static UT_MAP_DEFINE(netdev, trace_reg);
static UT_MAP_DEFINE(netdev, trace_open);
int stub_net_core_dev_error(void)
{
	return UT_MAP_CLR(netdev, trace_reg) + UT_MAP_CLR(netdev, trace_open);
}

int dev_get_valid_name(struct net *net, struct net_device *dev,
		       const char *name)
{
	if (dev->name != name)
		strncpy(dev->name, name, IFNAMSIZ);

	return 0;
}
EXPORT_SYMBOL(dev_get_valid_name);

static int netif_alloc_netdev_queues(struct net_device *dev)
{
	unsigned int count = dev->num_tx_queues;
	struct netdev_queue *tx;
	size_t sz = count * sizeof(*tx);

	if (count < 1 || count > 0xffff)
		return -EINVAL;

	tx = kzalloc(sz, GFP_KERNEL | __GFP_NOWARN | __GFP_REPEAT);
	if (!tx) {
		tx = vzalloc(sz);
		if (!tx)
			return -ENOMEM;
	}
	dev->_tx = tx;

	//netdev_for_each_tx_queue(dev, netdev_init_one_queue, NULL);
	spin_lock_init(&dev->tx_global_lock);

	return 0;
}

static int netif_alloc_rx_queues(struct net_device *dev)
{
	unsigned int i, count = dev->num_rx_queues;
	struct netdev_rx_queue *rx;
	size_t sz = count * sizeof(*rx);

	BUG_ON(count < 1);

	rx = kzalloc(sz, GFP_KERNEL | __GFP_NOWARN | __GFP_REPEAT);
	if (!rx) {
		rx = vzalloc(sz);
		if (!rx)
			return -ENOMEM;
	}
	dev->_rx = rx;

	for (i = 0; i < count; i++)
		rx[i].dev = dev;
	return 0;
}

/**
 *	register_netdev	- register a network device
 *	@dev: device to register
 *
 *	Take a completed network device structure and add it to the kernel
 *	interfaces. A %NETDEV_REGISTER message is sent to the netdev notifier
 *	chain. 0 is returned on success. A negative errno code is returned
 *	on a failure to set up the device, or if the name is a duplicate.
 *
 *	This is a wrapper around register_netdevice that takes the rtnl semaphore
 *	and expands the device name if you passed a format string to
 *	alloc_netdev.
 */
 
int register_netdev(struct net_device *dev)
{
	UT_MAP_INSERT(netdev, trace_reg, dev, dev);

	dev->reg_state = NETREG_REGISTERED;

	return 0;
}

EXPORT_SYMBOL(register_netdev);

/**
 *	unregister_netdev - remove device from the kernel
 *	@dev: device
 *
 *	This function shuts down a device interface and removes it
 *	from the kernel tables.
 *
 *	This is just a wrapper for unregister_netdevice that takes
 *	the rtnl semaphore.  In general you want to use this and not
 *	unregister_netdevice.
 */
void unregister_netdev(struct net_device *dev)
{
	UT_MAP_ERASE(netdev, trace_reg, dev);

	dev->reg_state = NETREG_UNREGISTERED;
}
EXPORT_SYMBOL(unregister_netdev);


/**
 *	dev_open	- prepare an interface for use.
 *	@dev:	device to open
 *
 *	Takes a device from down to up state. The device's private open
 *	function is invoked and then the multicast lists are loaded. Finally
 *	the device is moved into the up state and a %NETDEV_UP message is
 *	sent to the netdev notifier chain.
 *
 *	Calling this function on an active interface is a nop. On a failure
 *	a negative errno code is returned.
 */
int dev_open(struct net_device *dev)
{
	UT_MAP_INSERT(netdev, trace_open, dev, dev);

	return 0;
}
EXPORT_SYMBOL(dev_open);

/**
 *	dev_close - shutdown an interface.
 *	@dev: device to shutdown
 *
 *	This function moves an active device into down state. A
 *	%NETDEV_GOING_DOWN is sent to the netdev notifier chain. The device
 *	is then deactivated and finally a %NETDEV_DOWN is sent to the notifier
 *	chain.
 */
int dev_close(struct net_device *dev)
{
	UT_MAP_ERASE(netdev, trace_open, dev);
	return 0;
}
EXPORT_SYMBOL(dev_close);

/**
 * alloc_netdev_mqs - allocate network device
 * @sizeof_priv: size of private data to allocate space for
 * @name: device name format string
 * @name_assign_type: origin of device name
 * @setup: callback to initialize device
 * @txqs: the number of TX subqueues to allocate
 * @rxqs: the number of RX subqueues to allocate
 *
 * Allocates a struct net_device with private data area for driver use
 * and performs basic initialization.  Also allocates subqueue structs
 * for each queue on the device.
 */
struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name,
		unsigned char name_assign_type,
		void (*setup)(struct net_device *),
		unsigned int txqs, unsigned int rxqs)
{
	struct net_device *dev;
	unsigned int alloc_size;
	struct net_device *p;
	int i;

	alloc_size = sizeof(struct net_device);
	if (sizeof_priv) {
		/* ensure 32-byte alignment of private area */
		alloc_size = ALIGN(alloc_size, NETDEV_ALIGN);
		alloc_size += sizeof_priv;
	}
	/* ensure 32-byte alignment of whole construct */
	alloc_size += NETDEV_ALIGN - 1;

	//p = kvzalloc(alloc_size, GFP_KERNEL | __GFP_RETRY_MAYFAIL);
	p = kzalloc(alloc_size, GFP_KERNEL);
	if (!p)
		return NULL;
	
	dev = p;
	dev->dev_addr = kzalloc(MAX_ADDR_LEN, 0);

	dev->num_tx_queues = txqs;
	dev->real_num_tx_queues = txqs;
	dev->_tx = kzalloc(txqs * sizeof(struct netdev_queue), 0);
	for (i = 0; i < txqs; i++)
		dev->_tx[i].dev = dev;
	
	dev->num_rx_queues = rxqs;
	dev->real_num_rx_queues = rxqs;
	dev->_rx = kzalloc(rxqs * sizeof(struct netdev_queue), 0);
	for (i = 0; i < rxqs; i++)
		dev->_rx[i].dev = dev;


	dev_mc_init(dev);
	dev_uc_init(dev);
	
	return dev;
}
EXPORT_SYMBOL(alloc_netdev_mqs);

/**
 * free_netdev - free network device
 * @dev: device
 *
 * This function does the last stage of destroying an allocated device
 * interface. The reference to the device object is released. If this
 * is the last reference then it will be freed.Must be called in process
 * context.
 */
void free_netdev(struct net_device *dev)
{
	if (!dev)
		return;

	if (dev->dev_addr)
		kvfree(dev->dev_addr);
	
	if (dev->_rx)
		kvfree(dev->_rx);
	
	if (dev->_tx)
		kvfree(dev->_tx);

	kvfree(dev);
}
EXPORT_SYMBOL(free_netdev);


/*
int netdev_set_tc_queue(struct net_device *dev, u8 tc, u16 count, u16 offset)
{
	dev->tc_to_txq[tc].count = count;
	dev->tc_to_txq[tc].offset = offset;
	return 0;
}
EXPORT_SYMBOL(netdev_set_tc_queue);


void netdev_reset_tc(struct net_device *dev)
{
	dev->num_tc = 0;
	memset(dev->tc_to_txq, 0, sizeof(dev->tc_to_txq));
	memset(dev->prio_tc_map, 0, sizeof(dev->prio_tc_map));
}
EXPORT_SYMBOL(netdev_reset_tc);

int netdev_set_num_tc(struct net_device *dev, u8 num_tc)
{
	dev->num_tc = num_tc;
	return 0;
}
EXPORT_SYMBOL(netdev_set_num_tc);
*/

/*
 * Invalidate hardware checksum when packet is to be mangled, and
 * complete checksum manually on outgoing path.
 */
int skb_checksum_help(struct sk_buff *skb)
{
	return 0;
}
EXPORT_SYMBOL(skb_checksum_help);


void __dev_kfree_skb_any(struct sk_buff *skb, enum skb_free_reason reason)
{

}
EXPORT_SYMBOL(__dev_kfree_skb_any);

bool napi_complete_done(struct napi_struct *n, int work_done)
{
	unsigned long flags, val, new;

	/*
	 * 1) Don't let napi dequeue from the cpu poll list
	 *    just in case its running on a different cpu.
	 * 2) If we are busy polling, do nothing here, we have
	 *    the guarantee we will be called later.
	 */
	return true;
}
EXPORT_SYMBOL(napi_complete_done);

gro_result_t napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	return 0;
}
EXPORT_SYMBOL(napi_gro_receive);



void napi_disable(struct napi_struct *n)
{
	set_bit(NAPI_STATE_DISABLE, &n->state);

	clear_bit(NAPI_STATE_DISABLE, &n->state);
}
EXPORT_SYMBOL(napi_disable);


void netif_napi_add(struct net_device *dev, struct napi_struct *napi,
		    int (*poll)(struct napi_struct *, int), int weight)
{
 	napi->poll = poll;
	if (weight > NAPI_POLL_WEIGHT)
		pr_err_once("netif_napi_add() called with weight %d on device %s\n",
			    weight, dev->name);
	napi->weight = weight;
}
EXPORT_SYMBOL(netif_napi_add);

/* Must be called in process context */
void netif_napi_del(struct napi_struct *napi)
{
	napi->gro_list = NULL;
	napi->gro_count = 0;
}
EXPORT_SYMBOL(netif_napi_del);


/**
 * __napi_schedule - schedule for receive
 * @n: entry to schedule
 *
 * The entry's receive function will be scheduled to run.
 * Consider using __napi_schedule_irqoff() if hard irqs are masked.
 */
void __napi_schedule(struct napi_struct *n)
{

}
EXPORT_SYMBOL(__napi_schedule);



/**
 *	napi_schedule_prep - check if napi can be scheduled
 *	@n: napi context
 *
 * Test if NAPI routine is already running, and if not mark
 * it as running.  This is used as a condition variable
 * insure only one NAPI poll instance runs.  We also make
 * sure there is no pending NAPI disable.
 */
/*
bool napi_schedule_prep(struct napi_struct *n)
{
	return 1;
}
EXPORT_SYMBOL(napi_schedule_prep);
*/


void netif_schedule_queue(struct netdev_queue *txq)
{
}
EXPORT_SYMBOL(netif_schedule_queue);


void netif_tx_wake_queue(struct netdev_queue *dev_queue)
{

}
EXPORT_SYMBOL(netif_tx_wake_queue);

/*
 * Routine to help set real_num_tx_queues. To avoid skbs mapped to queues
 * greater then real_num_tx_queues stale skbs on the qdisc must be flushed.
 */
int netif_set_real_num_tx_queues(struct net_device *dev, unsigned int txq)
{
	return 0;
}
EXPORT_SYMBOL(netif_set_real_num_tx_queues);

/**
 *	netif_set_real_num_rx_queues - set actual number of RX queues used
 *	@dev: Network device
 *	@rxq: Actual number of RX queues
 *
 *	This must be called either with the rtnl_lock held or before
 *	registration of the net device.  Returns 0 on success, or a
 *	negative error code.  If called before registration, it always
 *	succeeds.
 */
int netif_set_real_num_rx_queues(struct net_device *dev, unsigned int rxq)
{
	return 0;
}
EXPORT_SYMBOL(netif_set_real_num_rx_queues);

void netif_tx_stop_all_queues(struct net_device *dev)
{

}
EXPORT_SYMBOL(netif_tx_stop_all_queues);

static int napi_gro_complete(struct sk_buff *skb)
{
	struct packet_offload *ptype;
	__be16 type = skb->protocol;
	//struct list_head *head = &offload_base;
	int err = -ENOENT;

	BUILD_BUG_ON(sizeof(struct napi_gro_cb) > sizeof(skb->cb));

	if (NAPI_GRO_CB(skb)->count == 1) {
		skb_shinfo(skb)->gso_size = 0;
		//goto out;
	}

	return 0;
}

void napi_gro_flush(struct napi_struct *napi, bool flush_old)
{
    ;
}
EXPORT_SYMBOL(napi_gro_flush);

int register_netdevice_notifier(struct notifier_block *nb)
{
	return 0;

}
EXPORT_SYMBOL(register_netdevice_notifier);

int unregister_netdevice_notifier(struct notifier_block *nb)
{
	return 0;
}
EXPORT_SYMBOL(unregister_netdevice_notifier);

void netdev_put(struct net_device *dev)
{

}
EXPORT_SYMBOL(netdev_put);

void netdev_hold(struct net_device *dev)
{

}
EXPORT_SYMBOL(netdev_hold);


//UT_MAP_DEFINE(vlan_dev_vlan_id, proc);
u16 vlan_dev_vlan_id(const struct net_device* dev)
{
    return 0;//(u16)UT_MAP_FIND(vlan_dev_vlan_id, proc, dev);
}
