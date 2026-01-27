#ifndef _STUB_COMMON_H_
#define _STUB_COMMON_H_

#include "stub_bits.h"
#include "stub_mem.h"
#include "stub_lock.h"
#include "stub_irq.h"
#include "stub_pci.h"
#include "stub_dma.h"
#include "stub_io.h"
#include "stub_dev.h"
#include "stub_ib.h"

#ifndef PRINTF
#define PRINTF(fmt, ...)	\
	printf("[Line: %04d. %s] " fmt,  __LINE__, __func__, ## __VA_ARGS__)
#endif

#define stub_setup_common() 0
#define stub_teardown_common() do { \
	UT_EXPECT_INT_EQ(0, stub_spinlock_error()); \
	UT_EXPECT_INT_EQ(0, stub_mutex_error()); \
	UT_EXPECT_INT_EQ(0, stub_pci_common_error()); \
	UT_EXPECT_INT_EQ(0, stub_pci_irq_error()); \
	UT_EXPECT_INT_EQ(0, stub_page_common_error()); \
	UT_EXPECT_INT_EQ(0, stub_page_swap_error()); \
	UT_EXPECT_INT_EQ(0, stub_net_core_dev_error()); \
	UT_EXPECT_INT_EQ(0, stub_io_common_error()); \
	ut_mem_error();/* donot move, this item must the last one */ \
} while(0)

#endif
