/*
 * lib/bitmap.c
 * Helper functions for bitmap.h.
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */
#include "ut_lib.h"

 
#include <linux/export.h>
#include <linux/thread_info.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include <asm/page.h>

/**
 * DOC: bitmap introduction
 *
 * bitmaps provide an array of bits, implemented using an an
 * array of unsigned longs.  The number of valid bits in a
 * given bitmap does _not_ need to be an exact multiple of
 * BITS_PER_LONG.
 *
 * The possible unused bits in the last, partially used word
 * of a bitmap are 'don't care'.  The implementation makes
 * no particular effort to keep them zero.  It ensures that
 * their value will not affect the results of any operation.
 * The bitmap operations that return Boolean (bitmap_empty,
 * for example) or scalar (bitmap_weight, for example) results
 * carefully filter out these unused bits from impacting their
 * results.
 *
 * These operations actually hold to a slightly stronger rule:
 * if you don't input any bitmaps to these ops that have some
 * unused bits set, then they won't output any set unused bits
 * in output bitmaps.
 *
 * The byte ordering of bitmaps is more natural on little
 * endian architectures.  See the big-endian headers
 * include/asm-ppc64/bitops.h and include/asm-s390/bitops.h
 * for the best explanations of this ordering.
 */

int __bitmap_weight(const unsigned long *bitmap, unsigned int bits)
{
	unsigned int k, lim = bits/BITS_PER_LONG;
	int w = 0;

	for (k = 0; k < lim; k++)
		w += hweight_long(bitmap[k]);

	if (bits % BITS_PER_LONG)
		w += hweight_long(bitmap[k] & BITMAP_LAST_WORD_MASK(bits));

	return w;
}
EXPORT_SYMBOL(__bitmap_weight);

/*
 * This is a common helper function for find_next_bit, find_next_zero_bit, and
 * find_next_and_bit. The differences are:
 *  - The "invert" argument, which is XORed with each fetched word before
 *    searching it for one bits.
 *  - The optional "addr2", which is anded with "addr1" if present.
 */
static inline unsigned long _find_next_bit(const unsigned long *addr1,
		const unsigned long *addr2, unsigned long nbits,
		unsigned long start, unsigned long invert)
{
	unsigned long tmp;

	if (unlikely(start >= nbits))
		return nbits;

	tmp = addr1[start / BITS_PER_LONG];
	if (addr2)
		tmp &= addr2[start / BITS_PER_LONG];
	tmp ^= invert;

	/* Handle 1st word. */
	tmp &= BITMAP_FIRST_WORD_MASK(start);
	start = round_down(start, BITS_PER_LONG);

	while (!tmp) {
		start += BITS_PER_LONG;
		if (start >= nbits)
			return nbits;

		tmp = addr1[start / BITS_PER_LONG];
		if (addr2)
			tmp &= addr2[start / BITS_PER_LONG];
		tmp ^= invert;
	}

	return min(start + __ffs(tmp), nbits);
}

UT_CNT_RANGE_DEFINE(find_first_zero_bit, proc);
UT_MAP_DEFINE(find_first_zero_bit, proc);
unsigned long find_first_zero_bit(const unsigned long *addr,
					 unsigned long size)
{
	unsigned long idx;
	int cnt = 0;
	if (UT_CNT_RANGE_CHECK_AND_GET(find_first_zero_bit, proc, 1, &cnt)) {
		return (unsigned long)UT_MAP_FIND(find_first_zero_bit,  proc, cnt);
	}

	for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
		if (addr[idx] != ~0UL)
			return min(idx * BITS_PER_LONG + ffz(addr[idx]), size);
	}

	return size;
}

UT_CNT_RANGE_DEFINE(bitmap_find_next_zero_area_off, proc);
UT_MAP_DEFINE(bitmap_find_next_zero_area_off, proc);
unsigned long bitmap_find_next_zero_area_off(unsigned long *map,
						    unsigned long size,
						    unsigned long start,
						    unsigned int nr,
						    unsigned long align_mask,
						    unsigned long align_offset)
{
	int cnt = 0;
	unsigned long index, end, i;

	if (UT_CNT_RANGE_CHECK_AND_GET(bitmap_find_next_zero_area_off, proc, 1, &cnt)) {
		return (unsigned long)UT_MAP_FIND(bitmap_find_next_zero_area_off,  proc, cnt);
	}

again:
	index = _find_next_bit(map, NULL, size, start, ~0UL);

	/* Align allocation */
	index = __ALIGN_MASK(index + align_offset, align_mask) - align_offset;

	end = index + nr;
	if (end > size)
		return end;
	i = _find_next_bit(map, NULL, end, index, 0UL);
	if (i < end) {
		start = i + 1;
		goto again;
	}
	return index;
}


UT_CNT_RANGE_DEFINE(find_next_bit, proc);
UT_MAP_DEFINE(find_next_bit, proc);
unsigned long find_next_bit(const unsigned long *addr, unsigned long
		size, unsigned long offset)
{
	int cnt = 0;
	if (UT_CNT_RANGE_CHECK_AND_GET(find_next_bit, proc, 1, &cnt)) {
		return (unsigned long)UT_MAP_FIND(find_next_bit,  proc, cnt);
	}

	return _find_next_bit(addr, NULL, size, offset, 0UL);
}

UT_CNT_RANGE_DEFINE(find_first_bit, proc);
UT_MAP_DEFINE(find_first_bit, proc);
unsigned long find_first_bit(const unsigned long *addr,
				    unsigned long size)
{
	int cnt = 0;
	if (UT_CNT_RANGE_CHECK_AND_GET(find_first_bit, proc, 1, &cnt)) {
		return (unsigned long)UT_MAP_FIND(find_first_bit,  proc, cnt);
	}

	return _find_next_bit(addr, NULL, size, 0, 0UL);
}

UT_CNT_RANGE_DEFINE(find_next_zero_bit, proc);
UT_MAP_DEFINE(find_next_zero_bit, proc);
unsigned long find_next_zero_bit(const unsigned long *addr, unsigned
		long size, unsigned long offset)
{
	int cnt = 0;
	if (UT_CNT_RANGE_CHECK_AND_GET(find_next_zero_bit, proc, 1, &cnt)) {
		return (unsigned long)UT_MAP_FIND(find_next_zero_bit,  proc, cnt);
	}

	return _find_next_bit(addr, NULL, size, offset, ~0UL);
}

