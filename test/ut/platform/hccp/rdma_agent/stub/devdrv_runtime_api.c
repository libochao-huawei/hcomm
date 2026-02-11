/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ascend_hal.h"
#include "dsmi_common_interface.h"
#include "dl_hal_function.h"

int dsmi_get_device_ip_address(int device_id, int port_type, int port_id, ip_addr_t *ip_address,
                               ip_addr_t *mask_address)
{
	return 0;
}

int dev_read_flash(unsigned int dev_id, const char *name, unsigned char *buf, unsigned int *buf_len)
{
   return 0;
}

int halSetUserConfig(unsigned int dev_id, const char *name, unsigned char *buf, unsigned int buf_size)
{
    return 0;
}

int halClearUserConfig(unsigned int devid, const char *name)
{
    return 0;
}
