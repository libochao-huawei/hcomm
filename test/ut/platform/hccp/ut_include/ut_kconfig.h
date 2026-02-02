/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * unit test config header
 *
 * this file will overwrite the kconfig.h 
 */

#undef CONFIG_NR_CPUS
#define CONFIG_NR_CPUS 1

/* debug list  */
#define CONFIG_DEBUG_LIST 1

#define CONFIG_OF 1
#define CONFIG_OF_ADDRESS 1

/* enable Kernel virtual address for page_address stub */
#define WANT_PAGE_VIRTUAL   1

#define CONFIG_KASAN   1
#define CONFIG_KASAN_SHADOW_OFFSET 0

#define CONFIG_PCI 1
#define CONFIG_PCI_IOV 1
#define CONFIG_GENERIC_MSI_IRQ 1
#define CONFIG_DCB 1

#define CONFIG_HNS3_DCB 1
#define CONFIG_HCLGE_DCB 1

//Hi1980
//#define CONFIG_DEBUG_LIST 1
#undef CONFIG_DEBUG_LIST
