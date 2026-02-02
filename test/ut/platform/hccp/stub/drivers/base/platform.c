#include <linux/platform_device.h>
#include "ut_lib.h"

/**
 * platform_get_resource - get a resource for a device
 * @dev: platform device
 * @type: resource type
 * @num: resource index
 */
struct resource *platform_get_resource(struct platform_device *dev,
				       unsigned int type, unsigned int num)
{
	int i;

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *r = &dev->resource[i];

		if (type == resource_type(r) && num-- == 0) {
			return r;
		}
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(platform_get_resource);
UT_ENTRY_STUB(platform_get_resource, platform_get_resource);
UT_RETURN_STUB(platform_get_resource, NULL);


/**
 * __platform_driver_register - register a driver for platform-level devices
 * @drv: platform driver structure
 * @owner: owning module/driver
 */
int __platform_driver_register(struct platform_driver *drv,
			       struct module *owner)
{
	drv->driver.owner = owner;


	return 0;
}
EXPORT_SYMBOL_GPL(__platform_driver_register);

/**
 * platform_driver_unregister - unregister a driver for platform-level devices
 * @drv: platform driver structure
 */
void platform_driver_unregister(struct platform_driver *drv)
{

}
EXPORT_SYMBOL_GPL(platform_driver_unregister);
