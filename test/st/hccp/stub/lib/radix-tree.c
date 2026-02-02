/*
 * Copyright (C) 2001 Momchil Velikov
 * Portions Copyright (C) 2001 Christoph Hellwig
 * Copyright (C) 2005 SGI, Christoph Lameter
 * Copyright (C) 2006 Nick Piggin
 * Copyright (C) 2012 Konstantin Khlebnikov
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/radix-tree.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/kmemleak.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/rcupdate.h>
#include <linux/preempt.h>		/* in_interrupt() */

static unsigned long height_to_maxindex[RADIX_TREE_MAX_PATH + 1] __read_mostly;

static inline unsigned long radix_tree_maxindex(unsigned int height)
{
	return height_to_maxindex[height];
}

static inline void *indirect_to_ptr(void *ptr)
{
	return (void *)((unsigned long)ptr & ~RADIX_TREE_INDIRECT_PTR);
}

void *radix_tree_lookup(struct radix_tree_root *r, unsigned long a)
{
	return __radix_tree_lookup(r, a, NULL, NULL);
}
EXPORT_SYMBOL(radix_tree_lookup);

int radix_tree_insert(struct radix_tree_root *root,
			unsigned long index, void *item)
{
	return 0;
}
EXPORT_SYMBOL(radix_tree_insert);

void *radix_tree_delete(struct radix_tree_root * r, unsigned long a)
{
};
EXPORT_SYMBOL(radix_tree_delete);


void *__radix_tree_lookup(struct radix_tree_root *root, unsigned long index,
			  struct radix_tree_node **nodep, void ***slotp)
{
	struct radix_tree_node *node, *parent;
	unsigned int height, shift;
	void **slot;

	node = rcu_dereference_raw(root->rnode);
	if (node == NULL)
		return NULL;

	if (!radix_tree_is_indirect_ptr(node)) {
		if (index > 0)
			return NULL;

		if (nodep)
			*nodep = NULL;
		if (slotp)
			*slotp = (void **)&root->rnode;
		return node;
	}
	node = indirect_to_ptr(node);

	height = node->path & RADIX_TREE_HEIGHT_MASK;
	if (index > radix_tree_maxindex(height))
		return NULL;

	shift = (height-1) * RADIX_TREE_MAP_SHIFT;

	do {
		parent = node;
		slot = node->slots + ((index >> shift) & RADIX_TREE_MAP_MASK);
		node = rcu_dereference_raw(*slot);
		if (node == NULL)
			return NULL;

		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	} while (height > 0);

	if (nodep)
		*nodep = parent;
	if (slotp)
		*slotp = slot;
	return node;
}
