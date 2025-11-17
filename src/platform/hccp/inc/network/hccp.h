/**
 * @file hccp.h
 * @brief This module provides APIs sockets and rdma operations for HCCL
 * @version Copyright (c) Huawei Technologies Co., Ltd. 2019-2025. All rights reserved.
 * @date 2019-03-25
 */

#ifndef HCCP_H
#define HCCP_H

#include "hccp_common.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @ingroup libsocket
 * @brief Client sockets batch connect to server sockets(async)
 * @param conn [IN] client sockets array
 * @param num [IN] num of conn
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_batch_connect(struct socket_connect_info_t conn[], unsigned int num);

/**
 * @ingroup libsocket
 * @brief Sockets batch close
 * @param conn [IN] sockets array, use disuse_linger of the fist conn as the common attr for all
 * @param num [IN] num of conn
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_batch_close(struct socket_close_info_t conn[], unsigned int num);

/**
 * @ingroup libsocket
 * @brief Client sockets batch abort connect to server sockets
 * @param conn [IN] client sockets array
 * @param num [IN] num of conn
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_batch_abort(struct socket_connect_info_t conn[], unsigned int num);

/**
 * @ingroup libsocket
 * @brief Sockets batch listen
 * @param conn [IN/OUT] server info array
 * @param num [IN] num of conn
 * @attention check if IP exist when SOCK_EADDRNOTAVAIL is returned
 * check if IP has been listened when SOCK_EADDRINUSE is returned
 * one IP address can only be listened once
 * @see ra_socket_listen_stop
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_listen_start(struct socket_listen_info_t conn[], unsigned int num);

/**
 * @ingroup libsocket
 * @brief Sockets batch stop
 * @param conn [IN] sockets info array
 * @param num [IN] num of conn
 * @see ra_socket_listen_start
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_listen_stop(struct socket_listen_info_t conn[], unsigned int num);

/**
 * @ingroup libsocket
 * @brief Get socket info of connected socket
 * @param role [IN] 0:server 1:client
 * @param conn [IN/OUT] connection info of sockets
 * @param connected_num [OUT] num of connected sockets
 * @attention if connected_num is zero or greater than zero but less than num when return value is zero,
   the function needed to be revoked again until the total connected_num is equal to num
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_sockets(unsigned int role, struct socket_info_t conn[], unsigned int num,
    unsigned int *connected_num);

/**
 * @ingroup libsocket
 * @brief Send data by fd handle
 * @param fd_handle [IN] fd handle
 * @param data [IN] send storage buff
 * @param size [IN] size of data you want to send unit(Byte)
 * @param sent_size [OUT] number of sent bytes
 * @see ra_socket_recv
 * @attention if sent_size is greate than zero but less than size,
 * the function needed to be revoked again,
 * the param of size is original size minus sent_size,
  *the param of data should also offset by sent_size
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_send(const void *fd_handle, const void *data, unsigned long long size,
    unsigned long long *sent_size);

/**
 * @ingroup libsocket
 * @brief Receive data by fd handle
 * @param fd_handle [IN] fd handle
 * @param data [IN/OUT] receive storage buff
 * @param size [IN] size of data you want to receive unit(Byte)
 * @param received_size [OUT] number of received bytes
 * @see ra_socket_send
 * @attention if return value is SOCK_EAGAIN which means no data right now,
 * you need to revoke the funcion again
 * @retval #zero Success
 * @retval #SOCK_EAGAIN Success(no data received by socket)
 * @retval #non-zero Failure(exclude SOCK_EAGAIN)
*/
HCCP_ATTRI_VISI_DEF int ra_socket_recv(const void *fd_handle, void *data, unsigned long long size,
    unsigned long long *received_size);

/**
 * @ingroup libsocket
 * @brief Get client sockets error info
 * @param conn [IN] client sockets array
 * @param err [OUT] sockets error info array
 * @param num [IN] num of conn and err
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_client_socket_err_info(struct socket_connect_info_t conn[],
    struct socket_err_info err[], unsigned int num);

/**
 * @ingroup libsocket
 * @brief Get server sockets error info
 * @param conn [IN] server info array
 * @param err [OUT] sockets error info array
 * @param num [IN] num of conn and err
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_server_socket_err_info(struct socket_listen_info_t conn[],
    struct server_socket_err_info err[], unsigned int num);

/**
 * @ingroup libsocket
 * @brief Add epoll listening event
 * @param fd_handle [IN] fd handle
 * @param event [IN] epoll event
 * @see ra_epoll_ctl_del
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_epoll_ctl_add(const void *fd_handle, enum RaEpollEvent event);

/**
 * @ingroup libsocket
 * @brief Modify epoll listening event
 * @param fd_handle [IN] fd handle
 * @param event [IN] epoll event
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_epoll_ctl_mod(const void *fd_handle, enum RaEpollEvent event);

/**
 * @ingroup libsocket
 * @brief Delete epoll listening event
 * @param fd_handle [IN] fd handle
 * @param event [IN] epoll event
 * @see ra_epoll_ctl_add
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_epoll_ctl_del(const void *fd_handle);

/**
 * @ingroup libsocket
 * @brief Set hook function for epoll listening events
 * @param socket_handle [IN] socket handle
 * @param callback [IN] hook function
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_set_tcp_recv_callback(const void *socket_handle, const void *callback);

/**
 * @ingroup librdma
 * @brief Create qp handle(only one qp handle)
 * @param rdev_handle [IN] rdev_handle
 * @param flag [IN] type of qp(reserved)
 * @param qp_mode [IN] mode to create qp
 * @param qp_handle [OUT] qp handle
 * @see ra_qp_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_qp_create(void *rdev_handle, int flag, int qp_mode, void **qp_handle);

/**
 * @ingroup librdma
 * @brief Create qp handle(only one qp handle)
 * @param rdev_handle [IN] rdev_handle
 * @param ext_attrs [IN] qp ext attrs
 * @param qp_handle [OUT] qp handle
 * @see ra_qp_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_qp_create_with_attrs(void *rdev_handle, struct qp_ext_attrs *ext_attrs, void **qp_handle);

/**
 * @ingroup librdma
 * @brief Create qp handle(only one qp handle)
 * @param rdev_handle [IN] rdev_handle
 * @param attrs [IN] ai qp attrs
 * @param info [OUT] ai qp info
 * @param qp_handle [OUT] qp handle
 * @see ra_qp_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ai_qp_create(void *rdev_handle, struct qp_ext_attrs *attrs, struct ai_qp_info *info,
    void **qp_handle);

/**
 * @ingroup librdma
 * @brief Create loopback qp handle(only one qp handle)
 * @param rdev_handle [IN] rdev_handle
 * @param qp_pair [OUT] loopback qp pair
 * @param qp_handle [OUT] qp handle
 * @see ra_qp_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_loopback_qp_create(void *rdev_handle, struct loopback_qp_pair *qp_pair, void **qp_handle);

/**
 * @ingroup librdma
 * @brief Destroy qp handle
 * @param qp_handle [IN] QP handle
 * @see ra_qp_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_qp_destroy(void *qp_handle);

/**
 * @ingroup librdma
 * @brief Build QP chain by socket(exchange qp and mr info by socket)
 * revoke ra_get_qp_status to get qp async status
 * @param qp_handle [IN] QP handle
 * @param fd_handle [IN] fd handle
 * @see ra_get_qp_status
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_qp_connect_async(void *qp_handle, const void *fd_handle);

/**
 * @ingroup librdma
 * @brief Create qp handle(only one qp handle)
 * @param rdev_handle [IN] rdev_handle
 * @param flag [IN] type of qp(reserved)
 * @param qp_mode [IN] mode to create qp
 * @param qp_info [OUT] qp info
 * @param qp_handle [OUT] qp handle
 * @see ra_qp_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_typical_qp_create(void *rdev_handle, int flag, int qp_mode, struct typical_qp *qp_info,
    void **qp_handle);

/**
 * @ingroup librdma
 * @brief Modify qp handle step by step from init to rts
 * @param qp_handle [IN] local qp handle
 * @param local_qp_info [IN] local qp info
 * @param remote_qp_info [IN] remote qp info
 * @see ra_typical_qp_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_typical_qp_modify(void *qp_handle, struct typical_qp *local_qp_info,
    struct typical_qp *remote_qp_info);

/**
 * @ingroup librdma
 * @brief Get qp async stats after revoking ra_qp_connect_async function
 * @param qp_handle [IN] qp handle
 * @param status [IN/OUT] qp connection status
 * 0:not connected, 1:connected, 2:timeout, 3:connecting
 * @see ra_qp_connect_async
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_qp_status(void *qp_handle, int *status);

/**
 * @ingroup librdma
 * @brief Create comp channel
 * @param rdma_handle [IN] rdma handle
 * @param comp_channel [OUT] comp channel
 * @see ra_destroy_comp_channel
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_create_comp_channel(const void *rdma_handle, void **comp_channel);

/**
 * @ingroup librdma
 * @brief Destroy comp channel
 * @param rdma_handle [IN] rdma handle
 * @param comp_channel [IN] comp channel
 * @see ra_create_comp_channel
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_destroy_comp_channel(const void *rdma_handle, void *comp_channel);

/**
 * @ingroup librdma
 * @brief Create cq
 * @param rdev_handle [IN] rdev handle
 * @param attr [OUT] cq attr
 * @see ra_cq_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_cq_create(void *rdev_handle, struct cq_attr *attr);

/**
 * @ingroup librdma
 * @brief Destory cq
 * @param rdev_handle [IN] rdev handle
 * @param attr [IN] cq attr
 * @see ra_cq_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_cq_destroy(void *rdev_handle, struct cq_attr *attr);

/**
 * @ingroup librdma
 * @brief Create qp handle(only one qp handle)
 * @param rdev_handle [IN] rdev handle
 * @param qp_init_attr [IN] qp attr
 * @param qp_handle [OUT] qp handle
  * @param qp [OUT] qp resource
 * @see ra_normal_qp_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_normal_qp_create(void *rdev_handle, struct ibv_qp_init_attr *qp_init_attr, void **qp_handle,
    void **qp);

/**
 * @ingroup librdma
 * @brief Destroy qp handle
 * @param qp_handle [IN] qp handle
 * @see ra_normal_qp_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_normal_qp_destroy(void *qp_handle);

/**
 * @ingroup librdma
 * @brief Register mr
 * @param qp_handle [IN] qp handle
 * @param info [IN/OUT] mr info
 * @see ra_mr_dereg
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_mr_reg(void *qp_handle, struct mr_info *info);

/**
 * @ingroup librdma
 * @brief Deregister mr
 * @param qp_handle [IN] qp handle
 * @param info [IN] mr info
 * @see ra_mr_reg
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_mr_dereg(void *qp_handle, struct mr_info *info);

/**
 * @ingroup librdma
 * @brief Register mr
 * @param rdma_handle [IN] rdma handle
 * @param info [IN/OUT] mr info
 * @param mr_handle [OUT] mr handle
 * @see ra_deregister_mr
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_register_mr(const void *rdma_handle, struct mr_info *info, void **mr_handle);

/**
 * @ingroup librdma
 * @brief Remap mr
 * @param rdma_handle [IN] rdma handle
 * @param info [IN] mem info list of mr
 * @param num [IN] num of info, max num of input is REMAP_MR_MAX_NUM
 * @see ra_register_mr
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_remap_mr(const void *rdma_handle, struct mem_remap_info info[], unsigned int num);

/**
 * @ingroup librdma
 * @brief Deregister mr
 * @param rdma_handle [IN] rdma handle
 * @param mr_handle [IN] handle
 * @see ra_register_mr
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_deregister_mr(const void *rdma_handle, void *mr_handle);

/**
 * @ingroup librdma
 * @brief Send RDMA packet async
 * @param qp_handle [IN] qp handle
 * @param wr [IN] work request
 * @param op_rsp [IN/OUT] respond of sending work request
 * @attention if return value is SOCK_ENOENT which means mr async not success right now,
 * you need to revoke the funcion again
 * @retval #zero Success
 * @retval #SOCK_ENOENT Success
 * @retval #non-zero Failure(exclude SOCK_ENOENT)
*/
HCCP_ATTRI_VISI_DEF int ra_send_wr(void *qp_handle, struct send_wr *wr, struct send_wr_rsp *op_rsp);

/**
 * @ingroup librdma
 * @brief Send RDMA packet async v2
 * @param qp_handle [IN] qp handle
 * @param wr [IN] work request
 * @param op_rsp [IN/OUT] respond of sending work request
 * @attention if return value is SOCK_ENOENT which means mr async not success right now,
 * you need to revoke the funcion again
 * @retval #zero Success
 * @retval #SOCK_ENOENT Success
 * @retval #non-zero Failure(exclude SOCK_ENOENT)
*/
HCCP_ATTRI_VISI_DEF int ra_send_wr_v2(void *qp_handle, struct send_wr_v2 *wr, struct send_wr_rsp *op_rsp);

/**
 * @ingroup librdma
 * @brief Send RDMA packet async with typical qp
 * @param qp_handle [IN] qp handle
 * @param wr [IN] work request
 * @param op_rsp [IN/OUT] respond of sending work request
 * @retval #zero Success
 * @retval #SOCK_ENOENT Success
 * @retval #non-zero Failure(exclude SOCK_ENOENT)
*/
HCCP_ATTRI_VISI_DEF int ra_typical_send_wr(void *qp_handle, struct send_wr *wr, struct send_wr_rsp *op_rsp);

/**
 * @ingroup librdma
 * @brief Send RDMA request async
 * @param qp_handle [IN] qp handle
 * @param wr [IN] work request list
 * @param op_rsp [IN/OUT] respond list of sending work request
 * @param send_num [IN] size of wr list
 * @param complete_num [OUT] number of wr been sent successfully
 * @attention if return value is SOCK_ENOENT which means mr async not success right now,
 * you need to revoke the funcion again
 * @retval #zero Success
 * @retval #SOCK_ENOENT Success
 * @retval #non-zero Failure(exclude SOCK_ENOENT)
*/
HCCP_ATTRI_VISI_DEF int ra_send_wrlist(void *qp_handle, struct send_wrlist_data wr[], struct send_wr_rsp op_rsp[],
    unsigned int send_num, unsigned int *complete_num);

/**
 * @ingroup librdma
 * @brief Send RDMA request async
 * @param qp_handle [IN] qp handle
 * @param wr [IN] work request list (ext)
 * @param op_rsp [IN/OUT] respond list of sending work request
 * @param send_num [IN] size of wr list
 * @param complete_num [OUT] number of wr been sent successfully
 * @attention if return value is SOCK_ENOENT which means mr async not success right now,
 * you need to revoke the funcion again
 * @retval #zero Success
 * @retval #SOCK_ENOENT Success
 * @retval #non-zero Failure(exclude SOCK_ENOENT)
*/
HCCP_ATTRI_VISI_DEF int ra_send_wrlist_ext(void *qp_handle, struct send_wrlist_data_ext wr[],
    struct send_wr_rsp op_rsp[], unsigned int send_num, unsigned int *complete_num);

/**
 * @ingroup librdma
 * @brief Send RDMA request async
 * @param qp_handle [IN] qp handle
 * @param wr [IN] work request list
 * @param op_rsp [IN/OUT] respond list of sending work request
 * @param send_num [IN] size of wr list
 * @param complete_num [OUT] number of wr been sent successfully
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_send_normal_wrlist(void *qp_handle, struct wr_info wr[], struct send_wr_rsp op_rsp[],
    unsigned int send_num, unsigned int *complete_num);

/**
 * @ingroup librdma
 * @brief Get notify base address and size
 * @param rdev_handle [IN] rdev handle
 * @param va [IN/OUT] address of notify
 * @param size [IN/OUT] size of notify
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_notify_base_addr(void *rdev_handle, unsigned long long *va, unsigned long long *size);

/**
 * @ingroup librdma
 * @brief Get notify mr info
 * @param rdev_handle [IN] rdev handle
 * @param info [OUT] mr info
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_notify_mr_info(void *rdev_handle, struct mr_info *info);

/**
 * @ingroup libinit
 * @brief Rdma_agent initialization
 * @param config [IN] initialization configuration of rdma_agent
 * @see ra_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_init(struct ra_init_config *config);

/**
 * @ingroup libcommon
 * @brief get tls enable info
 * @param info [IN] see ra_info
 * @param tls_enable [OUT] tls enable. true: enable, false: disable
 * @see ra_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_tls_enable(struct ra_info *info, bool *tls_enable);

/**
 * @ingroup libcommon
 * @brief ra get sec random
 * @param info [IN] see ra_info
 * @param value [OUT] sec random value
 * @see ra_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_sec_random(struct ra_info *info, uint32_t *value);

/**
 * @ingroup libinit
 * @brief Rdma_agent deinitialization
 * @param config [IN] deinitialization configuration of rdma_agent
 * @see ra_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_deinit(struct ra_init_config *config);

/**
 * @ingroup libinit
 * @brief Rdma_agent socket initialization
 * @param mode [IN] network mode
 * @param rdev_info [IN] rdev_info including dev_id and ip info
 * @param socket_handle [OUT] socket handle info
 * @see ra_socket_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_init(int mode, struct rdev rdev_info, void **socket_handle);

/**
 * @ingroup libinit
 * @brief Rdma_agent socket initialization
 * @param mode [IN] network mode
 * @param socket_init_info_t [IN] socket_init including dev_id and ip info
 * @param socket_handle [OUT] socket handle info
 * @see ra_socket_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_init_v1(int mode, struct socket_init_info_t socket_init, void **socket_handle);

/**
 * @ingroup libinit
 * @brief Rdma_agent socket deinitialization
 * @param socket_handle [IN] deinitialization handle of socket
 * @see ra_socket_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_deinit(void *socket_handle);

/**
 * @ingroup libinit
 * @brief Rdma_agent rdev initialization will start lite thread by default
 * @param mode [IN] network mode
 * @param notify_type [IN] notify type
 * @param rdev_info [IN] rdev_info including dev_id and ip info
 * @param rdma_handle [OUT] rdma_handle rdma handle info
 * @see ra_rdev_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_rdev_init(int mode, unsigned int notify_type, struct rdev rdev_info, void **rdma_handle);

/**
 * @ingroup libinit
 * @brief Rdma_agent rdev initialization
 * @param init_info [IN] init info, can customize to start lite thread or not
 * @param rdev_info [IN] rdev_info including dev_id and ip info
 * @param rdma_handle [OUT] rdma_handle rdma handle info
 * @see ra_rdev_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_rdev_init_v2(struct rdev_init_info init_info, struct rdev rdev_info, void **rdma_handle);

/**
 * @ingroup libinit
 * @brief Rdma_agent rdev initialization with backup
 * @param init_info [IN] init info, can customize to start lite thread or not
 * @param rdev_info [IN] rdev_info including dev_id and ip info
 * @param backup_rdev_info [IN] backup_rdev_info including dev_id and ip info
 * @param rdma_handle [OUT] rdma_handle rdma handle info related to rdev_info
 * @see ra_rdev_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_rdev_init_with_backup(struct rdev_init_info *init_info, struct rdev *rdev_info,
    struct rdev *backup_rdev_info, void **rdma_handle);

/**
 * @ingroup libinit
 * @brief Rdma_agent rdev deinitialization
 * @param rdma_handle [IN] deinitialization handle of rdev
 * @param notify_type [IN] notify type
 * @see ra_rdev_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_rdev_deinit(void *rdma_handle, unsigned int notify_type);

/**
 * @ingroup libinit
 * @brief Rdma_agent get support lite
 * @param rdma_handle [IN] rdma handle
 * @param support_lite [OUT] rdma lite support(0: not support lite, 1: 4KB page align lite, 2: 2MB page align lite)
 * @see ra_rdev_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_rdev_get_support_lite(void *rdma_handle, int *support_lite);

/**
 * @ingroup libsocket
 * @brief set socket whitelist status
 * @param enable [IN] enable or disable
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_set_white_list_status(unsigned int enable);

/**
 * @ingroup libsocket
 * @brief get socket whitelist status
 * @param enable [OUT] enable or disable
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_get_white_list_status(unsigned int *enable);

/**
 * @ingroup libsocket
 * @brief Add server socket whitelist
 * @param socket_handle [IN] socket handle
 * @param white_list [IN] server's whitelist
 * @param num [IN] num of whitelist
 * @see ra_socket_white_list_del
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_white_list_add(void *socket_handle, struct socket_wlist_info_t white_list[],
    unsigned int num);

/**
 * @ingroup libsocket
 * @brief Remove server socket whitelist
 * @param socket_handle [IN] socket handle
 * @param white_list [IN] server's whitelist
 * @param num [IN] num of whitelist
 * @see ra_socket_white_list_add
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_white_list_del(void *socket_handle, struct socket_wlist_info_t white_list[],
    unsigned int num);

/**
 * @ingroup libsocket
 * @brief Sockets batch listen
 * @param conn [IN] server info array
 * @param num [IN] num of conn
 * @param credit_limit [IN] credit limit
 * @attention once set accept credit, once it exhausted listen_fd will be ctl_del.
 * credit need to be add again to ctl_add listen_fd to handle accept events
 * @see ra_socket_listen_start
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_accept_credit_add(struct socket_listen_info_t conn[], unsigned int num,
    unsigned int credit_limit);

/**
 * @ingroup libinit
 * @brief get the number of interfaces
 * @param config [IN] config
 * @param num [OUT] num of interfaces
 * @see ra_get_ifnum
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_ifnum(struct ra_get_ifattr *config, unsigned int *num);

/**
 * @ingroup libinit
 * @brief get interface ips by device id
 * @param config [IN] config
 * @param interface_infos [OUT] ip result
 * @param num [IN/OUT] num of param and num of param found
 * @see ra_get_ifaddrs
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_ifaddrs(struct ra_get_ifattr *config, struct interface_info interface_infos[],
    unsigned int *num);

/**
 * @ingroup libinit
 * @brief get vnic ip infos by corresponding id_type
 * @param phy_id [IN] phy id
 * @param type [IN] id type
 * @param ids [IN] id info, see id_type
 * @param num [IN] ids and infos array size
 * @param infos [OUT] ip infos
 * @see ra_socket_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_socket_get_vnic_ip_infos(unsigned int phy_id, enum id_type type, unsigned int ids[],
    unsigned int num, struct ip_info infos[]);

/**
 * @ingroup libcommon
 * @brief get interface version
 * @param interface_opcode [IN] interface opcode
 * @param phy_id [IN] phy id
 * @param interface_version [OUT] interface version
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_interface_version(unsigned int phy_id, unsigned int interface_opcode,
    unsigned int* interface_version);

/**
 * @ingroup librdma
 * @brief This function only invoked in asynchronous GDR scenario for more flexible shared memory partition,
 * it will set template depth and obtain max supported qp numbers. The template is a mechanism designed for
 * implementing asynchronous GDR. RoCE produce packets to template, and TS consumer packets from template.
 * @param rdev_handle [IN] rdev_handle
 * @param temp_depth [IN] template depth which decided by service requirements
 * @param qp_num [OUT] max supported numbers of qp
 * @see ra_get_tsqp_depth
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_set_tsqp_depth(void *rdev_handle, unsigned int temp_depth, unsigned int *qp_num);

/**
 * @ingroup librdma
 * @brief This function only invoked in asynchronous GDR scenario to get the template depth
 * and max supported qp numbers
 * @param rdev_handle [IN] rdev_handle
 * @param temp_depth [OUT] template depth
 * @param qp_num [OUT] max supported numbers of qp
 * @see ra_set_tsqp_depth
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_tsqp_depth(void *rdev_handle, unsigned int *temp_depth, unsigned int *qp_num);

/**
 * @ingroup librdma
 * @brief Rdma_agent get port status
 * @param rdma_handle [IN] rdma_handle
 * @param status [OUT] port status, see enum port_status
 * @see ra_rdev_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_rdev_get_port_status(void *rdma_handle, enum port_status *status);

/**
 * @ingroup librdma
 * @brief Post RDMA recv request async
 * @param qp_handle [IN] qp handle
 * @param wr [IN] recv work request list
 * @param recv_num [IN] size of wr list
 * @param complete_num [OUT] number of wr been post recv request successfully
 * @attention if return value is SOCK_ENOENT which means mr async not success right now,
 * you need to revoke the funcion again
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_recv_wrlist(void *qp_handle, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num);

/**
 * @ingroup librdma
 * @brief poll cq
 * @param qp_handle [IN] qp handle
 * @param is_send_cq [IN] true(send cq) or false(recv cq)
 * @param num_entries [IN] maximum number of completions to return
 * @param wc [OUT] array of at least @num_entries of &struct wc where completions
 * will be returned
 * @retval #non-negative Success it is the number of completions returned.
 * @retval #negative Failure
*/
HCCP_ATTRI_VISI_DEF int ra_poll_cq(void *qp_handle, bool is_send_cq, unsigned int num_entries, void *wc);

/**
 * @ingroup librdma
 * @brief get rdma qp context
 * @param rdev_handle [IN] qp_handle
 * @param qp [OUT] rdma qp
 * @param send_cq [OUT] rdma send cq
 * @param recv_cq [OUT] rdma recv cq
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_qp_context(void* qp_handle, void** qp, void** send_cq, void** recv_cq);

/**
 * @ingroup librdma
 * @brief set qos attr in qp
 * @param qp_handle [IN] qp handle
 * @param attr [IN] qp qos attr
 * @see ra_mr_dereg
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_set_qp_attr_qos(void *qp_handle, struct qos_attr *attr);

/**
 * @ingroup librdma
 * @brief set timeout attr in qp
 * @param qp_handle [IN] qp handle
 * @param attr [IN] qp timeout attr
 * @see ra_mr_dereg
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_set_qp_attr_timeout(void *qp_handle, unsigned int *timeout);

/**
 * @ingroup librdma
 * @brief set retry cnt attr in qp
 * @param qp_handle [IN] qp handle
 * @param attr [IN] qp qretry cntos attr
 * @see ra_mr_dereg
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_set_qp_attr_retry_cnt(void *qp_handle, unsigned int *retry_cnt);

/**
 * @ingroup librdma
 * @brief get cqe err info
 * @param phy_id [IN] phy id
 * @param info [IN/OUT] cqe err info
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_cqe_err_info(unsigned int phy_id, struct cqe_err_info *info);

/**
 * @ingroup librdma
 * @brief Rdma_agent get cqe err info by rdma_handle
 * @param rdma_handle [IN] rdma handle
 * @param info_list [IN/OUT] cqe err info
 * @param num [IN/OUT] num of cqe err info, max num of input is CQE_ERR_INFO_MAX_NUM
 * @see ra_rdev_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_rdev_get_cqe_err_info_list(void *rdma_handle, struct cqe_err_info *info_list,
    unsigned int *num);

/**
 * @ingroup librdma
 * @brief get qp attr from qp handle
 * @param qp_handle [IN] qp handle
 * @param attr [IN/OUT] qp attr, see qp_attr
 * @see ra_qp_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_qp_attr(void *qp_handle, struct qp_attr *attr);

/**
 * @ingroup librdma
 * @brief create srq by rdma handle
 * @param rdma_handle [IN] rdma handle,
 * @param rx_depth [IN] srq depth
 * @param srq_handle [IN/OUT] srq handle
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_create_srq(const void *rdma_handle, struct srq_attr *attr);

/**
 * @ingroup librdma
 * @brief create destroy by srq handle
 * @param srq_handle [IN] srq handle
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_destroy_srq(const void *rdma_handle, struct srq_attr *attr);

/**
 * @ingroup libsocket
 * @brief Create event handle
 * @param event_handle [OUT] event handle
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_create_event_handle(int *event_handle);

/**
 * @ingroup libsocket
 * @brief Control event handle
 * @param event_handle [IN] event handle
 * @param fd_handle [IN] fd handle
 * @param opcode [IN] valid opcodes to issue to sys_epoll_ctl
 * @param event [IN] epoll event
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctl_event_handle(int event_handle, const void *fd_handle, int opcode,
    enum RaEpollEvent event);

/**
 * @ingroup libsocket
 * @brief Wait event handle
 * @param event_handle [IN] event handle
 * @param event_infos [OUT] socket events
 * @param timeout [IN] epoll timeout
 * @param maxevents [IN] max socket events num
 * @param events_num [OUT] socket events num
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_wait_event_handle(int event_handle, struct socket_event_info *event_infos, int timeout,
    unsigned int maxevents, unsigned int *events_num);

/**
 * @ingroup libsocket
 * @brief Destroy event handle
 * @param event_handle [OUT] event handle
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_destroy_event_handle(int *event_handle);

/**
 * @ingroup librdma
 * @brief unimport remote mem
 * @param rdma_handle [IN] rdma handle
 * @param qp_handle [IN] qp handle
 * @param num [IN] qp handle num
 * @param expect_status [IN] expect_status
 * @see ra_get_qp_status
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_qp_batch_modify(void *rdma_handle, void *qp_handle[], unsigned int num, int expect_status);

/**
 * @ingroup librdma
 * @brief get rdma handle
 * @param phy_id [IN] phy id
 * @param rdma_handle [IN] rdma handle
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_rdev_get_handle(unsigned int phy_id, void **rdma_handle);

/**
 * @ingroup libcommon
 * @brief CRIU(Checkpoint/Restore in Userspace) save snapshot
 * @param info [IN] see ra_info
 * @param action [IN] see enum save_snapshot_action
 * @see ra_restore_snapshot
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_save_snapshot(struct ra_info *info, enum save_snapshot_action action);

/**
 * @ingroup libcommon
 * @brief CRIU(Checkpoint/Restore in Userspace) restore snapshot
 * @param info [IN] see ra_info
 * @see ra_save_snapshot
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_restore_snapshot(struct ra_info *info);
#ifdef __cplusplus
}
#endif
#endif // HCCP_H
