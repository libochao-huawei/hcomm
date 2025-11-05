/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _I2C_DRV_H_
#define _I2C_DRV_H_

#include <stdio.h>

typedef enum XSFP_IDENTIFIER {
    UNNKOWN = 0x00,
    GBIC = 0x01,
    SFP_SFPPLUS = 0x03,
    PIN_XBI = 0x04,
    XENPAK = 0x05,
    XFP = 0x06,
    XFF = 0x07,
    XFP_E = 0x08,
    XPAK = 0x09,
    X2 = 0x0A,
    DWDM_SFP_SFPPLUS = 0x0B,
    QSFP = 0x0C,
    QSFPPLUS = 0x0D,
    CXP = 0x0E,
    QSFP28 = 0x11,
    CXP2 = 0x12,
    RESERVED = -127
}xsfp_identifier;

struct xsfp_dev_info{
    int temp;
    unsigned int high_power_en;
};

/*
 * Description  :   Get sfp info
 * Parameter    :   [In] dev_id - Device ID
 *                  [In] port_id - Port ID
 *                  [OUT] xsfp_info - sfp info
 * Return       :   Success -0
 *                  Failure -Other Value
 */
int xsfp_get_info(int dev_id, int port_id, struct xsfp_dev_info *xsfp_info);

/*
 * Description  :   Get sfp present
 * Parameter    :   [In] dev_id - Device ID
 *                  [In] port_id - Port ID
 *                  [OUT] present - sfp present
 * Return       :   Success -0
 *                  Failure -Other Value
 */
int xsfp_get_present(int dev_id, int port_id, unsigned int *present);

/*
 * Description  :   Get sfp identifier/type
 * Parameter    :   [In] dev_id - Device ID
 *                  [In] port_id - Port ID
 *                  [OUT] optic_type - sfp identifier/type
 * Return       :   Success -0
 *                  Failure -Other Value
 */
int xsfp_get_identifier(int dev_id, int port_id, xsfp_identifier *optic_type);

/*
 * Description  :   Get 590x L2 firmware version
 * Parameter    :   [In] dev_id - Device ID
 *                  [In] port_id - Port ID
 *                  [In] fw_ver_len - firmware version string len, which should be larger than 10 bytes
 *                  [OUT] fw_ver -   retimer firmware version, format as a.b.c.d
 * Return       :   Success -0
 *                  Failure -Other Value
 */
int retimer_get_fw_version(int dev_id, int port_id, char fw_ver[], int fw_ver_len);

/*
 * Description  :   Set optical auto adapt speeds
 * Parameter    :   [In] dev_id - Device ID
 *                  [In] port_id - Port ID
 *                  [In] auto_adapt_flag - auto adapt flag
 * Return       :   Success -0
 *                  Failure -Other Value
 */
int xsfp_set_optical_autoadapt(int dev_id, int port_id, unsigned int auto_adapt_flag);

#endif
