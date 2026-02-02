/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * Description: rdma_service ccu external interface declaration
 * Create: 2024-04-29
 */

#ifndef RS_CCU_H
#define RS_CCU_H

#include "ccu_u_api.h"
#include "rs.h"

int RsCtxCcuCustomChannel(const struct channel_info_in *in, struct channel_info_out *out);
int RsCtxCcuMissionKill(unsigned int dieId);
int RsCtxCcuMissionDone(unsigned int dieId);
#endif // RS_CCU_H
