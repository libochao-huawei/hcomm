/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/pci.h>
#include <linux/platform_device.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>
#include "roce_k_compat.h"
#include "hns_roce_device.h"
#include "hns_roce_sec.h"
#include "hns_roce_ah.h"

static inline u16 get_ah_udp_sport(const struct rdma_ah_attr *ah_attr)
{
    u32 fl = ah_attr->grh.flow_label;
    u16 sport;

    if (!fl)
        sport = get_random_u32() %
            (IB_ROCE_UDP_ENCAP_VALID_PORT_MAX + 1 -
            IB_ROCE_UDP_ENCAP_VALID_PORT_MIN) +
            IB_ROCE_UDP_ENCAP_VALID_PORT_MIN;
    else
        sport = rdma_flow_label_to_udp_sport(fl);

    return sport;
}

static inline u8 get_tclass(const struct ib_global_route *grh)
{
    return grh->sgid_attr->gid_type == IB_GID_TYPE_ROCE_UDP_ENCAP ?
            grh->traffic_class >> DSCP_SHIFT : grh->traffic_class;
}

int hns_roce_create_ah(struct ib_ah *ibah, struct rdma_ah_init_attr *init_attr, struct ib_udata *udata)
{
    const struct ib_global_route *grh = NULL;
    struct rdma_ah_attr *ah_attr = NULL;
    struct hns_roce_dev *hr_dev = NULL;
    struct hns_roce_ah *ah = NULL;
    int ret = 0;

    if (ibah == NULL || init_attr == NULL) {
        pr_err("hns3: ibah[%pK] is null or init_attr[%pK] is null\n", ibah, init_attr);
        return -EINVAL;
    }

    ah_attr = init_attr->ah_attr;
    grh = rdma_ah_read_grh(ah_attr);
    hr_dev = to_hr_dev(ibah->device);
    ah = to_hr_ah(ibah);

    if (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08 && udata)
        return -EOPNOTSUPP;

    ah->av.port = rdma_ah_get_port_num(ah_attr);
    ah->av.gid_index = grh->sgid_index;

    if (rdma_ah_get_static_rate(ah_attr))
        ah->av.stat_rate = IB_RATE_10_GBPS;

    ah->av.hop_limit = grh->hop_limit;
    ah->av.flowlabel = grh->flow_label;
    ah->av.udp_sport = get_ah_udp_sport(ah_attr);
    ah->av.sl = rdma_ah_get_sl(ah_attr);
    ah->av.tclass = get_tclass(grh);

    memcpy_s(ah->av.dgid, HNS_ROCE_GID_SIZE, grh->dgid.raw, HNS_ROCE_GID_SIZE);
    memcpy_s(ah->av.mac, ETH_ALEN, ah_attr->roce.dmac, ETH_ALEN);

    /* HIP08 needs to record vlan info in Address Vector */
    if (hr_dev->pci_dev->revision == PCI_REVISION_ID_HIP08) {
        ret = rdma_read_gid_l2_fields(ah_attr->grh.sgid_attr, &ah->av.vlan, NULL);
        if (ret)
            return ret;

        ah->av.vlan_en = ah->av.vlan < VLAN_N_VID;
    }

    return ret;
}

int hns_roce_query_ah(struct ib_ah *ibah, struct rdma_ah_attr *ah_attr)
{
    struct hns_roce_ah *ah = NULL;
    int ret;

    if (ibah == NULL || ah_attr == NULL) {
        pr_err("hns3: query ah param err, ibah %pK, ah_attr %pK\n", ibah, ah_attr);
        return -EINVAL;
    }
    ah = to_hr_ah(ibah);
    ret = memset_s(ah_attr, sizeof(*ah_attr), 0, sizeof(*ah_attr));
    HNS_ROCE_SEC_CHECK_RET_INT_WITHOUT_DEV(ret);
    rdma_ah_set_sl(ah_attr, ah->av.sl);
    rdma_ah_set_port_num(ah_attr, ah->av.port);
    rdma_ah_set_static_rate(ah_attr, ah->av.stat_rate);
    rdma_ah_set_grh(ah_attr, NULL, ah->av.flowlabel, ah->av.gid_index, ah->av.hop_limit, ah->av.tclass);
    rdma_ah_set_dgid_raw(ah_attr, ah->av.dgid);

    return 0;
}

int hns_roce_destroy_ah(struct ib_ah *ah, u32 flags)
{
    return 0;
}
