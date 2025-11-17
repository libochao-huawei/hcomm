/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_HDC_SOCKET_H
#define RA_HDC_SOCKET_H
#include "ascend_hal.h"
#include "hccp_common.h"
#include "ra.h"
#include "ra_hdc.h"
#include "ra_rs_comm.h"

#define VNIC_IP_TYPE 0
#define RA_MAX_VNIC_NUM 16

struct close_fd_data {
    unsigned int phy_id;
    int close_fd;
};

union op_socket_init_data {
    struct {
        unsigned int vnic_ip[RA_MAX_VNIC_NUM];
        unsigned int num;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int rsvd;
    } rx_data;
};

union op_socket_deinit_data {
    struct {
        struct rdev rdev_info;
        unsigned int rsvd;
    } tx_data;

    struct {
        unsigned int rsvd;
    } rx_data;
};

union op_socket_connect_data {
    struct {
        unsigned int num;  // resv bit 31 for use_port on HDC, for compatibility issue
        struct socket_connect_info conn[MAX_SOCKET_NUM];
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_801];
    } rx_data;
};

union op_socket_close_data {
    struct {
        unsigned int num;  // resv bit 31 for disuse_linger on HDC, for compatibility issue
        struct close_fd_data conn[MAX_SOCKET_NUM];
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_33];
    } rx_data;
};

union op_socket_listen_data {
    struct {
        unsigned int phy_id;
        unsigned int num;  // resv bit 31 for use_port on HDC, for compatibility issue
        struct socket_listen_info conn[MAX_SOCKET_NUM];
    } tx_data;

    struct {
        unsigned int rsvd;
        struct socket_listen_info conn[MAX_SOCKET_NUM];
    } rx_data;
};

union op_socket_info_data {
    struct {
        unsigned int num;
        unsigned int role;
        struct socket_fd_data conn[MAX_SOCKET_NUM];
    } tx_data;

    struct {
        int num;
        unsigned int rsvd;
        struct socket_fd_data conn[MAX_SOCKET_NUM];
    } rx_data;
};

union op_socket_send_data {
    struct {
        unsigned int fd;
        unsigned int rsvd;
        unsigned long long send_size;
        char data_send[SOCKET_SEND_MAXLEN];
    } tx_data;

    struct {
        unsigned long long real_send_size;
        unsigned int rsvd[RA_RSVD_NUM_2];
    } rx_data;
};

union op_socket_recv_data {
    struct {
        unsigned int fd;
        unsigned int rsvd;
        unsigned long long recv_size;
    } tx_data;

    struct {
        unsigned long long real_recv_size;
        unsigned int rsvd[RA_RSVD_NUM_2];
    } rx_data;
};

union op_wlist_data {
    struct {
        struct rdev rdev_info;
        struct socket_wlist_info_t wlist[MAX_WLIST_NUM_V1];
        unsigned int num;
    } tx_data;

    struct {
    } rx_data;
};

union op_wlist_data_v2 {
    struct {
        struct rdev rdev_info;
        unsigned int num;
        struct socket_wlist_info_t wlist[MAX_WLIST_NUM];
    } tx_data;

    struct {
    } rx_data;
};

union op_accept_credit_data {
    struct {
        unsigned int phy_id;
        unsigned int credit_limit;
        unsigned int num;
        struct socket_listen_info conn[MAX_SOCKET_NUM];
    } tx_data;

    struct {
        unsigned int rsvd[RA_RSVD_NUM_64];
    } rx_data;
};

union op_ifaddr_data {
    struct {
        unsigned int phy_id;
        struct ifaddr_info ifaddr_infos[MAX_INTERFACE_NUM];
        unsigned int num;
    } tx_data;

    struct {
        struct ifaddr_info ifaddr_infos[MAX_INTERFACE_NUM];
        unsigned int num;
    } rx_data;
};

// support IPV4/IPV6
union op_ifaddr_data_v2 {
    struct {
        unsigned int phy_id;
        struct interface_info interface_infos[MAX_INTERFACE_NUM];
        unsigned int num;
    } tx_data;

    struct {
        struct interface_info interface_infos[MAX_INTERFACE_NUM];
        unsigned int num;
    } rx_data;
};

union op_get_vnic_ip_data {
    struct {
        unsigned int phy_id;
    } tx_data;

    struct {
        unsigned int vnic_ip;
    } rx_data;
};

union op_get_vnic_ip_infos_data_v1 {
    struct {
        unsigned int phy_id;
        enum id_type type;
        unsigned int ids[MAX_IP_INFO_NUM_V1];
        unsigned int num;
        unsigned int rsv;
    } tx_data;

    struct {
        struct ip_info infos[MAX_IP_INFO_NUM_V1];
        unsigned int rsv;
    } rx_data;
};

union op_get_vnic_ip_infos_data {
    struct {
        unsigned int phy_id;
        enum id_type type;
        unsigned int ids[MAX_IP_INFO_NUM];
        unsigned int num;
        unsigned int rsv;
    } tx_data;

    struct {
        struct ip_info infos[MAX_IP_INFO_NUM];
        unsigned int rsv;
    } rx_data;
};

int ra_hdc_socket_white_list_add(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num);
int ra_hdc_socket_white_list_del(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num);
int ra_hdc_socket_accept_credit_add(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num,
    unsigned int credit_limit);
int ra_hdc_socket_batch_connect(unsigned int phy_id, struct socket_connect_info_t conn[], unsigned int num);
int ra_hdc_socket_batch_close(unsigned int phy_id, struct socket_close_info_t conn[], unsigned int num);
int ra_hdc_socket_batch_abort(unsigned int phy_id, struct socket_connect_info_t conn[], unsigned int num);
int ra_hdc_socket_listen_start(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num);
int ra_hdc_socket_listen_stop(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num);
int ra_hdc_get_sockets(unsigned int phy_id, unsigned int role, struct socket_info_t conn[], unsigned int num);
int ra_hdc_socket_send(unsigned int phy_id, const void *handle, const void *data, unsigned long long size);
int ra_hdc_socket_recv(unsigned int phy_id, const void *handle, void *data, unsigned long long size);
int ra_hdc_socket_init(struct rdev rdev_info);
int ra_hdc_socket_deinit(struct rdev rdev_info);
int ra_hdc_get_ifnum(unsigned int phy_id, bool is_all, unsigned int *num);
int ra_hdc_get_ifaddrs(unsigned int phy_id, struct ifaddr_info ifaddr_infos[], unsigned int *num);
int ra_hdc_get_ifaddrs_v2(unsigned int phy_id, bool is_all, struct interface_info interface_infos[], unsigned int *num);
int ra_hdc_get_vnic_ip_infos_v1(unsigned int phy_id, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[]);
int ra_hdc_get_vnic_ip_infos(unsigned int phy_id, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[]);
#endif // RA_HDC_SOCKET_H
