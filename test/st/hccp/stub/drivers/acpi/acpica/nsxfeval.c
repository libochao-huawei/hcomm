#include <linux/acpi.h>

/*******************************************************************************
 *
 * FUNCTION:    acpi_evaluate_object
 *
 * PARAMETERS:  handle              - Object handle (optional)
 *              pathname            - Object pathname (optional)
 *              external_params     - List of parameters to pass to method,
 *                                    terminated by NULL. May be NULL
 *                                    if no parameters are being passed.
 *              return_buffer       - Where to put method's return value (if
 *                                    any). If NULL, no value is returned.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find and evaluate the given object, passing the given
 *              parameters if necessary. One of "Handle" or "Pathname" must
 *              be valid (non-null)
 *
 ******************************************************************************/
acpi_status
acpi_evaluate_object(acpi_handle handle,
		     acpi_string pathname,
		     struct acpi_object_list *external_params,
		     struct acpi_buffer *return_buffer)
{
	acpi_status status;
	return status;
}

ACPI_EXPORT_SYMBOL(acpi_evaluate_object)