/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _RDMA_DFX_H_
#define _RDMA_DFX_H_

#include <stdio.h>

/**
 * struct of device privilege authorization
 */
struct dev_pri_auth {
    int dev_id; /**< Device ID */
    int host_root; /**< status 0:not root 1:root */
    int run_env; /**< status 0:physical environment 1:not physical environment */
};

/*
 * Description  :   Initial rdfx info and create detection thread
 * Parameter    :   [In] device_id - Device ID
 * Return       :   Success -0
 *                  Failure -Other Value
 */
__attribute__ ((visibility ("default"))) int rdfx_init(int device_id);

/*
 * Description  :   Clear rdfx info and destroy detection thread
 * Parameter    :   [In] device_id - Device ID
 * Return       :   Success -0
 *                  Failure -Other Value
 */
__attribute__ ((visibility ("default"))) int rdfx_deinit(int device_id);

/*
 * Description  :   Get the device network connective status
 * Parameter    :   [In] device_id - Device ID
 *                  [Out] presult - Network connective status
 * Return       :   Success -0
 *                  Failure -Other Value
 */
__attribute__ ((visibility ("default"))) int rdfx_get_network_health(int device_id, unsigned int  *presult);

/*
 * Description  :   Get the Device info of Mac address/Gateway address etc.
 * Parameter    :   [In] auth - authorization of host
 *                  [In] inbuf -Request buffer include opcode and configuration messages
 *                  [In] size_in - The length of inbuf
 *                  [Out] outbuf - Response buffer include opcode and Device info
 *                  [Out] size_out - The length of outbuf
 * Return       :   Success -0
 *                  Failure -Other Value
 */
__attribute__ ((visibility ("default"))) int rdfx_trans_channel(struct dev_pri_auth *auth, const char *inbuf, unsigned int size_in,
    char *outbuf, unsigned int *size_out);

#endif
