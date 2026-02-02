#include "stdio.h"
#include <stdarg.h>

#include <linux/kern_levels.h>
#include <linux/device.h>
#include <linux/sysfs.h>

/**
 * device_create_file - create sysfs attribute file for device.
 * @dev: device.
 * @attr: device attribute descriptor.
 */
int device_create_file(struct device *dev,
		       const struct device_attribute *attr)
{
	int error = 0;

	if (dev) {
		WARN(((attr->attr.mode & S_IWUGO) && !attr->store),
			"Attribute %s: write permission without 'store'\n",
			attr->attr.name);
		WARN(((attr->attr.mode & S_IRUGO) && !attr->show),
			"Attribute %s: read permission without 'show'\n",
			attr->attr.name);
		error = sysfs_create_file(&dev->kobj, &attr->attr);
	}

	return error;
}
EXPORT_SYMBOL_GPL(device_create_file);

/**
 * device_remove_file - remove sysfs attribute file.
 * @dev: device.
 * @attr: device attribute descriptor.
 */
void device_remove_file(struct device *dev,
			const struct device_attribute *attr)
{
	if (dev)
		sysfs_remove_file(&dev->kobj, &attr->attr);
}
EXPORT_SYMBOL_GPL(device_remove_file);

#define define_dev_printk_level(func, kern_level)               \
	void func(const struct device *dev, const char *fmt, ...)       \
	{                                                               \
		va_list args;                                           \
		\
		va_start(args, fmt);                                    \
		vprintf(fmt, args);             \
		\
		va_end(args);                                           \
	}                                                               \
	EXPORT_SYMBOL(func);

define_dev_printk_level(dev_emerg, KERN_EMERG);
define_dev_printk_level(dev_alert, KERN_ALERT);
define_dev_printk_level(dev_crit, KERN_CRIT);
define_dev_printk_level(dev_err, KERN_ERR);
define_dev_printk_level(dev_warn, KERN_WARNING);
define_dev_printk_level(dev_notice, KERN_NOTICE);
define_dev_printk_level(_dev_info, KERN_INFO);