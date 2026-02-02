#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/export.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/pfn.h>
#include <linux/mm.h>
#include <linux/resource_ext.h>
#include <asm/io.h>


struct resource ioport_resource = {
	.name	= "PCI IO",
	.start	= 0,
	.end	= IO_SPACE_LIMIT,
	.flags	= IORESOURCE_IO,
};
EXPORT_SYMBOL(ioport_resource);

struct resource iomem_resource = {
	.name	= "PCI mem",
	.start	= 0,
	.end	= -1,
	.flags	= IORESOURCE_MEM,
};
EXPORT_SYMBOL(iomem_resource);

/*
 * For memory hotplug, there is no way to free resource entries allocated
 * by boot mem after the system is up. So for reusing the resource entry
 * we need to remember the resource.
 */
static struct resource *bootmem_resource_free;
static DEFINE_SPINLOCK(bootmem_resource_lock);

static struct resource *alloc_resource(gfp_t flags)
{
	struct resource *res = NULL;

	spin_lock(&bootmem_resource_lock);
	if (bootmem_resource_free) {
		res = bootmem_resource_free;
		bootmem_resource_free = res->sibling;
	}
	spin_unlock(&bootmem_resource_lock);

	if (res)
		memset(res, 0, sizeof(struct resource));
	else
		res = kzalloc(sizeof(struct resource), flags);

	return res;
}

/**
 * __request_region - create a new busy resource region
 * @parent: parent resource descriptor
 * @start: resource start address
 * @n: resource region size
 * @name: reserving caller's ID string
 * @flags: IO resource flags
 */
struct resource * __request_region(struct resource *parent,
				   resource_size_t start, resource_size_t n,
				   const char *name, int flags)
{
    struct resource *res = alloc_resource(GFP_KERNEL);
	return res;
}
EXPORT_SYMBOL(__request_region);

void __release_region(struct resource *parent, resource_size_t start,
			resource_size_t n)
{
	;
}
EXPORT_SYMBOL(__release_region);