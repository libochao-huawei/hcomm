/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * Description: rdma_service ccu external interface declaration
 * Create: 2024-04-29
 */

#ifndef RS_CCU_H
#define RS_CCU_H

#include "ccu_u_api.h"
#include "rs.h"

int rs_ctx_ccu_custom_channel(const struct channel_info_in *in, struct channel_info_out *out);
int rs_ctx_ccu_mission_kill(unsigned int die_id);
int rs_ctx_ccu_mission_done(unsigned int die_id);
#endif // RS_CCU_H
