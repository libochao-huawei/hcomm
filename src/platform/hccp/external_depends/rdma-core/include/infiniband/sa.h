/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INFINIBAND_SA_H
#define INFINIBAND_SA_H

#include <infiniband/verbs.h>
#include <linux/types.h>

struct ibv_sa_path_rec {
	/* reserved */
	/* reserved */
	union ibv_gid dgid;
	union ibv_gid sgid;
	__be16        dlid;
	__be16        slid;
	int           raw_traffic;
	/* reserved */
	__be32        flow_label;
	uint8_t       hop_limit;
	uint8_t       traffic_class;
	int           reversible;
	uint8_t       numb_path;
	__be16        pkey;
	/* reserved */
	uint8_t       sl;
	uint8_t       mtu_selector;
	uint8_t	      mtu;
	uint8_t       rate_selector;
	uint8_t       rate;
	uint8_t       packet_life_time_selector;
	uint8_t       packet_life_time;
	uint8_t       preference;
};

struct ibv_sa_mcmember_rec {
	union ibv_gid mgid;
	union ibv_gid port_gid;
	uint32_t      qkey;
	uint16_t      mlid;
	uint8_t       mtu_selector;
	uint8_t       mtu;
	uint8_t       traffic_class;
	uint16_t      pkey;
	uint8_t       rate_selector;
	uint8_t       rate;
	uint8_t       packet_life_time_selector;
	uint8_t       packet_life_time;
	uint8_t       sl;
	uint32_t      flow_label;
	uint8_t       hop_limit;
	uint8_t       scope;
	uint8_t       join_state;
	int           proxy_join;
};

struct ibv_sa_service_rec {
	uint64_t      id;
	union ibv_gid gid;
	uint16_t      pkey;
	/* uint16_t  resv;   */
	uint32_t      lease;
	uint8_t       key[16];
	uint8_t       name[64];
	uint8_t       data8[16];
	uint16_t      data16[8];
	uint32_t      data32[4];
	uint64_t      data64[2];
};

#define IBV_PATH_RECORD_REVERSIBLE 0x80

struct ibv_path_record {
	__be64		service_id;
	union ibv_gid	dgid;
	union ibv_gid	sgid;
	__be16		dlid;
	__be16		slid;
	__be32		flowlabel_hoplimit; /* resv-31:28 flow label-27:8 hop limit-7:0*/
	uint8_t		tclass;
	uint8_t		reversible_numpath; /* reversible-7:7 num path-6:0 */
	__be16		pkey;
	__be16		qosclass_sl;	    /* qos class-15:4 sl-3:0 */
	uint8_t		mtu;		    /* mtu selector-7:6 mtu-5:0 */
	uint8_t		rate;		    /* rate selector-7:6 rate-5:0 */
	uint8_t		packetlifetime;	    /* lifetime selector-7:6 lifetime-5:0 */
	uint8_t		preference;
	uint8_t		reserved[6];
};

#define IBV_PATH_FLAG_GMP	       (1<<0)
#define IBV_PATH_FLAG_PRIMARY	       (1<<1)
#define IBV_PATH_FLAG_ALTERNATE       (1<<2)
#define IBV_PATH_FLAG_OUTBOUND	       (1<<3)
#define IBV_PATH_FLAG_INBOUND	       (1<<4)
#define IBV_PATH_FLAG_INBOUND_REVERSE (1<<5)
#define IBV_PATH_FLAG_BIDIRECTIONAL   (IBV_PATH_FLAG_OUTBOUND |     \
					IBV_PATH_FLAG_INBOUND_REVERSE)

struct ibv_path_data {
	uint32_t		flags;
	uint32_t		reserved;
	struct ibv_path_record	path;
};

#endif /* INFINIBAND_SA_H */
