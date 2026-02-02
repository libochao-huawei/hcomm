
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/export.h>



/**
 * regmap_write() - Write a value to a single register
 *
 * @map: Register map to write to
 * @reg: Register to write to
 * @val: Value to be written
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int regmap_write(struct regmap *map, unsigned int reg, unsigned int val)
{
	int ret;


	return ret;
}
EXPORT_SYMBOL_GPL(regmap_write);

/**
 * regmap_read() - Read a value from a single register
 *
 * @map: Register map to read from
 * @reg: Register to be read from
 * @val: Pointer to store read value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val)
{
	int ret;

	return ret;
}
EXPORT_SYMBOL_GPL(regmap_read);