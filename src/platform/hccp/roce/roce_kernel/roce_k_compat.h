/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _ROCE_K_COMPAT_H
#define _ROCE_K_COMPAT_H

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#ifndef PCI_VENDOR_ID_HUAWEI
#define PCI_VENDOR_ID_HUAWEI        0x19e5
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))

#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0))
typedef unsigned long long __u64;

#if defined(__GNUC__)
typedef     __u64       uint64_t;
#endif

#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0))

#ifdef CONFIG_PCI_MSI
#include <linux/pci.h>
#include <linux/msi.h>
extern int memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
#define HNS_ROCE_K_SEC_CHECK_RET_VOID(ret) {if (ret) return ;}
#endif

#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))

#ifndef HAVE_LINUX_MM_H
#define HAVE_LINUX_MM_H
#endif

#ifndef HAVE_LINUX_SCHED_H
#define HAVE_LINUX_SCHED_H
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0))
#else /* >= 4.16 */
#ifndef HAVE_FILL_RES_ENTRY
#define HAVE_FILL_RES_ENTRY
#endif
#endif

typedef struct refcount_struct {
    atomic_t refs;
} refcount_t;

/**
 * refcount_set - set a refcount's value
 * @r: the refcount
 * @n: value to which the refcount will be set
 */
#undef refcount_set
#define refcount_set _kc_refcount_set
static inline void _kc_refcount_set(refcount_t *r, unsigned int n)
{
    atomic_set(&r->refs, n);
}

#if (LINUX_VERSION_CODE == KERNEL_VERSION(3, 10, 0))
#else
/**
 * Here we call kmalloc_array for mem allocate
 * Kernel optimize from 4.11
 */
#undef kvmalloc_array
#define kvmalloc_array _kc_kvmalloc_array
static inline void *_kc_kvmalloc_array(size_t n, size_t size, gfp_t flags)
{
    return kmalloc_array(n, size, flags);
}
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))

#if (LINUX_VERSION_CODE == KERNEL_VERSION(3, 10, 0))
#else
#undef addrconf_addr_eui48_base
#define addrconf_addr_eui48_base _kc_addrconf_addr_eui48_base
#define FIRSTSEG 3
#define NEXTSEG 5
#define SEGLEN 3
static inline void _kc_addrconf_addr_eui48_base(u8 *eui,
                                                const char *const addr)
{
    int ret = 0;
    ret = memcpy_s(eui, sizeof(u64), addr, SEGLEN);
    HNS_ROCE_K_SEC_CHECK_RET_VOID(ret);
    eui[FIRSTSEG] = 0xFF;
    eui[FIRSTSEG + 1] = 0xFE;
    ret = memcpy_s(eui + NEXTSEG, sizeof(u64) - NEXTSEG, addr + FIRSTSEG, SEGLEN);
    HNS_ROCE_K_SEC_CHECK_RET_VOID(ret);
}

#undef addrconf_addr_eui48
#define EUI_MASK    2
#define addrconf_addr_eui48 _kc_addrconf_addr_eui48
static inline void _kc_addrconf_addr_eui48(u8 *eui, const char *const addr)
{
    addrconf_addr_eui48_base(eui, addr);
    eui[0] ^= EUI_MASK;
}
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0))
#define is_signed_type(type)       (((type)(-1)) < (type)1)
#define __type_half_max(type) ((type)1 << (8*sizeof(type) - 1 - is_signed_type(type)))
#define type_max(T) ((T)((__type_half_max(T) - 1) + __type_half_max(T)))
#define type_min(T) ((T)((T)-type_max(T)-(T)1))

/*
 * If one of a or b is a compile-time constant, this avoids a division.
 */
#define __unsigned_mul_overflow(a, b, d, r) ({ do { \
    typeof(a) __a = (a);           \
    typeof(b) __b = (b);           \
    typeof(d) __d = (d);           \
    (void) (&__a == &__b);         \
    (void) (&__a == __d);          \
    *__d = __a * __b;              \
    r = __builtin_constant_p(__b) ? __b > 0 && __a > type_max(typeof(__a)) / __b : \
      __a > 0 && __b > type_max(typeof(__b)) / __a;  \
} while (0);})

/*
 * Signed multiplication is rather hard. gcc always follows C99, so
 * division is truncated towards 0. This means that we can write the
 * overflow check like this:
 *
 * The redundant casts of -1 are to silence an annoying -Wtype-limits
 * (included in -Wextra) warning: When the type is u8 or u16, the
 * __b_c_e in check_mul_overflow obviously selects
 * __unsigned_mul_overflow, but unfortunately gcc still parses this
 * code and warns about the limited range of __b.
 */
#define __signed_mul_overflow_apart(a, b, d, tmax, tmin, r) do {   \
        (void) (&(a) == &(b));  \
        (void) (&(a) == (d));   \
        *(d) = ((u64)(a)) * ((u64)(b));   \
        (r) = (((b) > 0   && ((a) > (tmax) / (b) || (a) < (tmin) / (b))) || \
        ((b) < (typeof(b)) - 1  && ((a) > (tmin) / (b) || (a) < (tmax) / (b))) || \
        ((b) == (typeof(b)) - 1 && (a) == (tmin)));   \
    } while (0)

#define __signed_mul_overflow(a, b, d, r) ({ do { \
        typeof(a) __a = (a);    \
        typeof(b) __b = (b);    \
        typeof(d) __d = (d);                        \
        typeof(a) __tmax = type_max(typeof(a));  \
        typeof(a) __tmin = type_min(typeof(a));             \
        __signed_mul_overflow_apart(__a, __b, __d, __tmax, __tmin, r);  \
    } while (0);})

#define check_mul_overflow(a, b, d, r)                  \
    __builtin_choose_expr(is_signed_type(typeof(a)),    \
            __signed_mul_overflow(a, b, d, r),          \
            __unsigned_mul_overflow(a, b, d, r))

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0))

#define SIZE_MAX    (~(size_t)0)
#endif

/**
 * array_size() - Calculate size of 2-dimensional array.
 *
 * @a: dimension one
 * @b: dimension two
 *
 * Calculates size of 2-dimensional array: @a * @b.
 *
 * Returns: number of bytes needed to represent the array or SIZE_MAX on
 * overflow.
 */
static inline __must_check size_t array_size(size_t a, size_t b)
{
    size_t bytes;
    int ret = 0;

    check_mul_overflow(a, b, &bytes, ret);
    if (ret) {
        return SIZE_MAX;
    }

    return bytes;
}
#endif

#define CONFIG_NEW_KERNEL
#define MODIFY_CQ_MASK

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
#ifndef dma_zalloc_coherent
#define dma_zalloc_coherent(d, s, h, f) dma_alloc_coherent(d, s, h, (f | __GFP_ZERO | __GFP_NOWARN))
#endif
#endif

#endif /* _ROCE_K_COMPAT_H */
