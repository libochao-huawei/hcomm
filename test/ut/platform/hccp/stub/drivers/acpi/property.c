#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/export.h>

/*
#define DECLARE_ACPI_FWNODE_OPS(ops) \
        const struct fwnode_operations ops

DECLARE_ACPI_FWNODE_OPS(acpi_device_fwnode_ops);
DECLARE_ACPI_FWNODE_OPS(acpi_data_fwnode_ops);

DECLARE_ACPI_FWNODE_OPS(acpi_device_fwnode_ops);
DECLARE_ACPI_FWNODE_OPS(acpi_data_fwnode_ops);
*/
/*
bool is_acpi_device_node(const struct fwnode_handle *fwnode)
{
        return !IS_ERR_OR_NULL(fwnode) &&
                fwnode->ops == &acpi_device_fwnode_ops;
}
EXPORT_SYMBOL(is_acpi_device_node);
*/
/*
bool is_acpi_data_node(const struct fwnode_handle *fwnode)
{
        return !IS_ERR_OR_NULL(fwnode) && fwnode->ops == &acpi_data_fwnode_ops;
}
EXPORT_SYMBOL(is_acpi_data_node);
*/
