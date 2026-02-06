/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: rdma service ccu interface
 * Create: 2024-04-29
 */

#include "user_log.h"
#include "dl_ccu_function.h"
#include "ra_rs_ctx.h"
#include "rs_ccu.h"

int RsCtxCcuCustomChannel(const struct channel_info_in *in, struct channel_info_out *out)
{
    return RsCcuCustomChannel(in, out);
}

STATIC int RsCcuMissionExec(unsigned int udieId, ccu_u_opcode_t ccuOp)
{
    struct channel_info_out chanOut = {0};
    struct channel_info_in chanIn = {0};
    int ret = 0;

    chanIn.data.data_info.udie_idx = udieId;
    chanIn.op = ccuOp;
    ret = RsCtxCcuCustomChannel(&chanIn, &chanOut);
    CHK_PRT_RETURN(ret != 0, hccp_run_warn("ccu_custom_channel unsuccessful, ccuOp[%u], ret[%d] udieId[%u]",
        ccuOp, ret, udieId), ret);
    CHK_PRT_RETURN(chanOut.op_ret != 0, hccp_run_warn("ccu_u_op unsuccessful, ccuOp[%u], op_ret[%d] udieId[%u]",
        ccuOp, chanOut.op_ret, udieId), chanOut.op_ret);

    return 0;
}

int RsCtxCcuMissionKill(unsigned int dieId)
{
    return RsCcuMissionExec(dieId, CCU_U_OP_SET_TASKKILL);
}

int RsCtxCcuMissionDone(unsigned int dieId)
{
    return RsCcuMissionExec(dieId, CCU_U_OP_CLEAN_TASKKILL_STATE);
}
