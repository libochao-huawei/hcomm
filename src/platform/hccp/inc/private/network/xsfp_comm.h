/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef XSFP_COMM_H
#define XSFP_COMM_H
#define XSFP_NAME_LEN             17
#define XSFP_OUI_LEN              3
#define XSFP_PN_LEN               17
#define XSFP_WAVE_LEN             2
#define XSFP_SN_LEN               17
#define XSFP_DATACODE_LEN         11
#define XSFP_POWER_LEN            8

struct xsfp_base_info {
    int wavelength;
    int xsfp_identifier;
    char vendor_sn[XSFP_SN_LEN];
    char vendor_pn[XSFP_PN_LEN];
    char vendor_oui[XSFP_OUI_LEN];
    char vendor_name[XSFP_NAME_LEN];
    char date_code[XSFP_DATACODE_LEN];
};

struct xsfp_additional_info {
    int voltage;
    char tx_power[XSFP_POWER_LEN];
    char rx_power[XSFP_POWER_LEN];
    int vcc_high_threshold;
    int vcc_low_threshold;
    int temp_high_threshold;
    int temp_low_threshold;
    int tx_power_high_threshold;
    int tx_power_low_threshold;
    int rx_power_high_threshold;
    int rx_power_low_threshold;
    char tx_bias[XSFP_POWER_LEN];
    int tx_los;
    int rx_los;
};
#endif