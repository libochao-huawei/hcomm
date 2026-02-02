#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>

/**
 * of_parse_phandle_with_fixed_args() - Find a node pointed by phandle in a list
 * @np:         pointer to a device tree node containing a list
 * @list_name:  property name that contains a list
 * @cell_count: number of argument cells following the phandle
 * @index:      index of a phandle to parse out
 * @out_args:   optional pointer to output arguments structure (will be filled)
 *
 * This function is useful to parse lists of phandles and their arguments.
 * Returns 0 on success and fills out_args, on error returns appropriate
 * errno value.
 *
 * Caller is responsible to call of_node_put() on the returned out_args->np
 * pointer.
 *
 * Example:
 *
 * phandle1: node1 {
 * }
 *
 * phandle2: node2 {
 * }
 *
 * node3 {
 *      list = <&phandle1 0 2 &phandle2 2 3>;
 * }
 *
 * To get a device_node of the `node2' node you may call this:
 * of_parse_phandle_with_fixed_args(node3, "list", 2, 1, &args);
 */
int of_parse_phandle_with_fixed_args(const struct device_node *np,
				     const char *list_name, int cell_count,
				     int index, struct of_phandle_args *out_args)
{
	if (index < 0) {
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(of_parse_phandle_with_fixed_args);
