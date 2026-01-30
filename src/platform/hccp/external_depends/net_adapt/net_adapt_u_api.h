/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: network adapter driver user APIs definitions
 * Create: 2025-11-4
 */
#ifndef NET_ADP_U_API_H
#define NET_ADP_U_API_H

#define NETADP_ATTRI_VISI_DEF __attribute__ ((visibility ("default")))

NETADP_ATTRI_VISI_DEF int net_adapt_init(void);
NETADP_ATTRI_VISI_DEF void net_adapt_uninit(void);
NETADP_ATTRI_VISI_DEF int net_alloc_jfc_id(const char *udev_name, unsigned int jfc_mode, unsigned int *jfc_id);
NETADP_ATTRI_VISI_DEF int net_free_jfc_id(const char *udev_name, unsigned int jfc_mode, unsigned int jfc_id);
NETADP_ATTRI_VISI_DEF int net_alloc_jetty_id(const char *udev_name, unsigned int jetty_mode, unsigned int *jetty_id);
NETADP_ATTRI_VISI_DEF int net_free_jetty_id(const char *udev_name, unsigned int jetty_mode, unsigned int jetty_id);
NETADP_ATTRI_VISI_DEF unsigned long long net_get_cqe_base_addr(unsigned int die_id);

#endif // NET_ADP_U_API_H
