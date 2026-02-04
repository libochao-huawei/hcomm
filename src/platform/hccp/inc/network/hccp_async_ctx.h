/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCP_ASYNC_CTX_H
#define HCCP_ASYNC_CTX_H

#include "hccp_common.h"
#include "hccp_ctx.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tp_attr {
    uint8_t retry_times_init : 3; // corresponding bitmap bit: 0
    uint8_t at : 5; // corresponding bitmap bit: 1
    uint8_t sip[16U]; // corresponding bitmap bit: 2
    uint8_t dip[16U]; // corresponding bitmap bit: 3
    uint8_t sma[6U]; // corresponding bitmap bit: 4
    uint8_t dma[6U]; // corresponding bitmap bit: 5
    uint16_t vlan_id : 12; // corresponding bitmap bit: 6
    uint8_t vlan_en : 1; // corresponding bitmap bit: 7
    uint8_t dscp : 6; // corresponding bitmap bit: 8
    uint8_t at_times : 5; // corresponding bitmap bit: 9
    uint8_t sl : 4; // corresponding bitmap bit: 10
    uint8_t ttl; // corresponding bitmap bit: 11
    uint8_t reserved[78];
};

struct get_tp_cfg {
    union get_tp_cfg_flag flag;
    enum transport_mode_t trans_mode;
    union hccp_eid local_eid;
    union hccp_eid peer_eid;
};

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief register local mem async
 * @param ctx_handle [IN] ctx handle
 * @param lmem_info [IN/OUT] lmem reg info
 * @param lmem_handle [OUT] lmem handle
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_ctx_lmem_unregister_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_lmem_register_async(void *ctx_handle, struct mr_reg_info_t *lmem_info,
    void **lmem_handle, void **req_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief unregister local mem async
 * @param ctx_handle [IN] ctx handle
 * @param lmem_handle [IN] lmem handle
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_ctx_lmem_register_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_lmem_unregister_async(void *ctx_handle, void *lmem_handle, void **req_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief create jetty/qp async
 * @param ctx_handle [IN] ctx handle
 * @param attr [IN] qp attr
 * @param info [OUT] qp info
 * @param qp_handle [OUT] qp handle
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_ctx_qp_destroy_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_create_async(void *ctx_handle, struct qp_create_attr *attr,
    struct qp_create_info *info, void **qp_handle, void **req_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief destroy jetty/qp async
 * @param qp_handle [IN] qp handle
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_ctx_qp_create_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_destroy_async(void *qp_handle, void **req_handle);

/**
 * @ingroup libudma
 * @brief batch destroy qp
 * @param ctx_handle [IN] ctx handle
 * @param qp_handle [IN] corresponding qp_handle list
 * @param num [IN/OUT] size of qp_handle list, max num is HCCP_MAX_QP_DESTROY_BATCH_NUM
 * @param req_handle [OUT] async request handle
 * @see ra_ctx_qp_create
 * @see ra_get_async_req_result
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_destroy_batch_async(void *ctx_handle, void *qp_handle[],
    unsigned int *num, void **req_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief import jetty/prepare rem_qp_handle for modify qp async
 * @param ctx_handle [IN] ctx handle
 * @param info [IN/OUT] qp import info
 * @param rem_qp_handle [OUT] remote qp handle
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_ctx_qp_unimport_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_import_async(void *ctx_handle, struct qp_import_info_t *info, void **rem_qp_handle,
    void **req_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief unimport jetty async
 * @param rem_qp_handle [IN] qp handle
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_ctx_qp_import_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_unimport_async(void *rem_qp_handle, void **req_handle);

/**
 * @ingroup libudma
 * @brief get tp info list
 * @param ctx_handle [IN] ctx handle
 * @param cfg [IN] get tp cfg
 * @param info_list [IN/OUT] corresponding tp info list
 * @param num [IN/OUT] size of info_list, max num is HCCP_MAX_TPID_INFO_NUM
 * @param req_handle [OUT] async request handle
 * @see ra_ctx_init
 * @see ra_get_async_req_result
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_tp_info_list_async(void *ctx_handle, struct get_tp_cfg *cfg, struct tp_info info_list[],
    unsigned int *num, void **req_handle);

/**
 * @ingroup libudma
 * @brief get corresponding eid by ip async
 * @param ctx_handle [IN] ctx handle
 * @param ip [IN] ip array, see struct IpInfo
 * @param eid [IN/OUT] eid array, see union hccp_eid
 * @param num [IN/OUT] num of ip and eid array, max num is GET_EID_BY_IP_MAX_NUM
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_ctx_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_eid_by_ip_async(void *ctx_handle, struct IpInfo ip[], union hccp_eid eid[],
    unsigned int *num, void **req_handle);

/**
 * @ingroup libudma
 * @brief get tp attr
 * @param ctx_handle [IN] ctx handle
 * @param tp_handle [IN] see struct tp_info
 * @param attr_bitmap [IN/OUT] see struct tp_attr
 * @param attr [IN/OUT] see struct tp_attr
 * @param req_handle [OUT] async request handle
 * @see ra_get_tp_info_list_async
 * @see ra_get_async_req_result
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_tp_attr_async(void *ctx_handle, uint64_t tp_handle, uint32_t *attr_bitmap,
    struct tp_attr *attr, void **req_handle);

/**
 * @ingroup libudma
 * @brief set tp attr
 * @param ctx_handle [IN] ctx handle
 * @param tp_handle [IN] see struct tp_info
 * @param attr_bitmap [IN] see struct tp_attr
 * @param attr [IN] see struct tp_attr
 * @param req_handle [OUT] async request handle
 * @see ra_get_tp_info_list_async
 * @see ra_get_async_req_result
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_set_tp_attr_async(void *ctx_handle, uint64_t tp_handle, uint32_t attr_bitmap,
    struct tp_attr *attr, void **req_handle);

#ifdef __cplusplus
}
#endif

#endif // HCCP_ASYNC_CTX_H
