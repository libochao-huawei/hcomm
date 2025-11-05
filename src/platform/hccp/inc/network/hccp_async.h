/**
 * @file hccp_async.h
 * @brief This module provides APIs async operations for HCCL
 * @version Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * @date 2025-02-17
 */
#ifndef HCCP_ASYNC_H
#define HCCP_ASYNC_H

#include "hccp_common.h"
#include "hccp_ctx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup libcommon
 * @brief check if async request is done, will free req_handle if it done
 * @param req_handle [IN] async request handle
 * @param req_result [OUT] async request return value
 * @retval #zero Success
 * @retval #OTHERS_EAGAIN try again
 * @retval #non-zero Failure(exclude OTHERS_EAGAIN)
*/
HCCP_ATTRI_VISI_DEF int ra_get_async_req_result(void *req_handle, int *req_result);

/**
 * @ingroup libsocket
 * @brief Client sockets batch connect to server sockets(async)
 * @param conn [IN] client sockets array
 * @param num [IN] num of conn
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_batch_connect_async(struct socket_connect_info_t conn[], unsigned int num,
    void **req_handle);

/**
 * @ingroup libsocket
 * @brief Sockets batch listen
 * @param conn [IN] server info array
 * @param num [IN] num of conn
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_socket_listen_stop_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_listen_start_async(struct socket_listen_info_t conn[], unsigned int num,
    void **req_handle);

/**
 * @ingroup libsocket
 * @brief Sockets batch stop
 * @param conn [IN sockets info array
 * @param num [IN] num of conn
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_socket_listen_start_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_listen_stop_async(struct socket_listen_info_t conn[], unsigned int num,
    void **req_handle);

/**
 * @ingroup libsocket
 * @brief Sockets batch close
 * @param conn [IN] sockets array, use disuse_linger of the fist conn as the common attr for all
 * @param num [IN] num of conn
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_batch_close_async(struct socket_close_info_t conn[], unsigned int num,
    void **req_handle);

/**
 * @ingroup libsocket
 * @brief Send data async by fd handle
 * @param fd_handle [IN] fd handle
 * @param data [IN] send storage buff
 * @param size [IN] size of data you want to send unit(Byte)
 * @param sent_size [IN/OUT] number of sent bytes
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_socket_recv_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_send_async(const void *fd_handle, const void *data, unsigned long long size,
    unsigned long long *sent_size, void **req_handle);

/**
 * @ingroup libsocket
 * @brief Receive data async by fd handle
 * @param fd_handle [IN] fd handle
 * @param data [IN/OUT] receive storage buff
 * @param size [IN] size of data you want to receive unit(Byte)
 * @param received_size [IN/OUT] number of received bytes
 * @param req_handle [OUT] async request handle
 * @see ra_get_async_req_result
 * @see ra_socket_send_async
 * @retval #zero Success
 * @retval #SOCK_EAGAIN Success(no data received by socket)
 * @retval #non-zero Failure(exclude SOCK_EAGAIN)
*/
HCCP_ATTRI_VISI_DEF int ra_socket_recv_async(const void *fd_handle, void *data, unsigned long long size,
    unsigned long long *received_size, void **req_handle);

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
#ifdef __cplusplus
}
#endif

#endif // HCCP_ASYNC_H
