/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "user_log.h"
#include "dl_ccu_function.h"
#include "ra_rs_ctx.h"
#include "rs_ccu.h"

int rs_ctx_ccu_custom_channel(const struct channel_info_in *in, struct channel_info_out *out)
{
    return rs_ccu_custom_channel(in, out);
}

STATIC int rs_ccu_mission_exec(unsigned int udie_id, ccu_u_opcode_t ccu_op)
{
    struct channel_info_out chan_out = {0};
    struct channel_info_in chan_in = {0};
    int ret = 0;

    chan_in.data.data_info.udie_idx = udie_id;
    chan_in.op = ccu_op;
    ret = rs_ctx_ccu_custom_channel(&chan_in, &chan_out);
    CHK_PRT_RETURN(ret != 0, hccp_run_warn("ccu_custom_channel unsuccessful, ccu_op[%u], ret[%d] udie_id[%u]",
        ccu_op, ret, udie_id), ret);
    CHK_PRT_RETURN(chan_out.op_ret != 0, hccp_run_warn("ccu_u_op unsuccessful, ccu_op[%u], op_ret[%d] udie_id[%u]",
        ccu_op, chan_out.op_ret, udie_id), chan_out.op_ret);

    return 0;
}

int rs_ctx_ccu_mission_kill(unsigned int die_id)
{
    return rs_ccu_mission_exec(die_id, CCU_U_OP_SET_TASKKILL);
}

int rs_ctx_ccu_mission_done(unsigned int die_id)
{
    return rs_ccu_mission_exec(die_id, CCU_U_OP_CLEAN_TASKKILL_STATE);
}
