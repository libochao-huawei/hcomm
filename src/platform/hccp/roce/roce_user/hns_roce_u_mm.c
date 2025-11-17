/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include "hns_roce_u.h"
#include "hns_roce_u_abi.h"
#include "hns_roce_u_sec.h"
#include "hns_roce_u_hw_v2_send.h"

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

#define PORT_NUM_MAX       24

int hns_roce_u_query_device(struct ibv_context *context,
                            const struct ibv_query_device_ex_input *input,
                            struct ibv_device_attr_ex *attr, size_t attr_size)
{
    int ret;
    size_t resp_size;
    uint64_t raw_fw_ver;
    unsigned int major, minor, sub_minor;
    struct ib_uverbs_ex_query_device_resp resp;

    resp_size = sizeof(resp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(context);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(attr);
    ret = ibv_cmd_query_device_any(context, input, attr, attr_size, &resp, &resp_size);
    if (ret) {
        roce_err("query device err [%d], expected 0", ret);
        return ret;
    }

    raw_fw_ver = resp.base.fw_ver;
    major = (raw_fw_ver >> RAW_FW_VERSION_32) & 0xffff;
    minor = (raw_fw_ver >> RAW_FW_VERSION_16) & 0xffff;
    sub_minor = raw_fw_ver & 0xffff;

    ret = snprintf_s(attr->orig_attr.fw_ver, sizeof(attr->orig_attr.fw_ver), sizeof(attr->orig_attr.fw_ver) - 1,
                     "%u.%u.%03u", major, minor, sub_minor);
    HNS_ROCE_U_SPRINTF_CHECK_RET_INT(ret);

    return 0;
}

int hns_roce_u_query_port(struct ibv_context *context, uint8_t port, struct ibv_port_attr *attr)
{
    struct ibv_query_port cmd;
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(context);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(attr);

    return ibv_cmd_query_port(context, port, attr, &cmd, sizeof(cmd));
}

struct ibv_pd *hns_roce_u_alloc_pd(struct ibv_context *context)
{
    struct ibv_alloc_pd cmd;
    /*lint -e429*/
    struct hns_roce_pd *pd = NULL;
    struct hns_roce_alloc_pd_resp resp = {};
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(context);

    pd = calloc(1, sizeof(struct hns_roce_pd));
    if (pd == NULL) {
        roce_err("calloc pd failed!");
        return NULL;
    }

    if (ibv_cmd_alloc_pd(context, &pd->ibv_pd, &cmd, sizeof(cmd),
                         (struct ib_uverbs_alloc_pd_resp *)&resp, sizeof(resp))) {
        roce_err("ibv cmd alloc pd failed");
        free(pd);
        pd = NULL;
        return NULL;
    }

    pd->pdn = resp.pdn;

    return &pd->ibv_pd;
    /*lint +e429*/
}

int hns_roce_u_free_pd(struct ibv_pd *pd)
{
    int ret;
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(pd);

    ret = ibv_cmd_dealloc_pd(pd);
    if (ret) {
        roce_warn("ibv_cmd_dealloc_pd err ret [%d], expect 0", ret);
        return ret;
    }

    free(to_hr_pd(pd));
    pd = NULL;
    return ret;
}

struct ibv_mr *hns_roce_u_reg_mr(struct ibv_pd *pd, void *addr, size_t length, uint64_t hca_va, int access)
{
    int ret;
    struct ibv_reg_mr cmd;
    struct verbs_mr *vmr = NULL;
    struct ib_uverbs_reg_mr_resp resp;
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(pd);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(addr);

    if (length == 0) {
        roce_err("length is 0!");
        return NULL;
    }

    vmr = calloc(1, sizeof(struct verbs_mr));
    if (vmr == NULL) {
        roce_err("calloc vmr failed!");
        return NULL;
    }

    ret = ibv_cmd_reg_mr(pd, addr, length, hca_va, access, vmr,
                         &cmd, sizeof(cmd), &resp, sizeof(resp));
    if (ret) {
        roce_err("reg mr failed, ret[%d]", ret);
        free(vmr);
        vmr = NULL;
        return NULL;
    }

    return &vmr->ibv_mr;
}

struct ibv_mr *hns_roce_u_exp_reg_mr(struct ibv_pd *pd, void *addr, size_t length,
                                     int access, struct roce_process_sign roce_sign)
{
    int ret;
    struct hns_roce_reg_mr cmd;
    struct verbs_mr *vmr = NULL;
    struct ib_uverbs_reg_mr_resp resp;
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(pd);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(addr);

    if (roce_sign.tgid == 0) {
        roce_err("host_pid parm is 0!");
        return NULL;
    }
    if (length == 0) {
        roce_err("length is 0!");
        return NULL;
    }

    cmd.psign.tgid = roce_sign.tgid;
    cmd.psign.devid = roce_sign.devid;
    cmd.psign.vfid = roce_sign.vfid;
    ret = strcpy_s(cmd.psign.sign, PROCESS_PSIZE_LENGTH, roce_sign.sign);
    if (ret) {
        roce_err("strcpy_s sign failed, ret[%d]", ret);
        return NULL;
    }

    vmr = calloc(1, sizeof(struct verbs_mr));
    if (vmr == NULL) {
        roce_err("calloc vmr failed!");
        return NULL;
    }

    ret = ibv_cmd_reg_mr(pd, addr, length, (uintptr_t)addr, access, vmr,
                         (struct ibv_reg_mr *)&(cmd), sizeof(cmd), &resp, sizeof(resp));
    if (ret) {
        roce_err("reg exp mr failed, ret[%d]", ret);
        free(vmr);
        vmr = NULL;
        return NULL;
    }

    return &vmr->ibv_mr;
}

int hns_roce_u_rereg_mr(struct verbs_mr *vmr, int flags, struct ibv_pd *pd,
                        void *addr, size_t length, int access)
{
    struct ibv_rereg_mr cmd;
    struct ib_uverbs_rereg_mr_resp resp;
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(pd);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(vmr);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(addr);

    return ibv_cmd_rereg_mr(vmr, flags, addr, length, (uintptr_t)addr,
                            access, pd, &cmd, sizeof(cmd), &resp,
                            sizeof(resp));
}

int hns_roce_u_dereg_mr(struct verbs_mr *vmr)
{
    int ret;
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(vmr);

    ret = ibv_cmd_dereg_mr(vmr);
    if (ret) {
        roce_err("ibv_cmd_dereg_mr err ret %d, expect 0", ret);
        return ret;
    }

    free(vmr);
    vmr = NULL;
    return ret;
}

int hns_roce_u_bind_mw(struct ibv_qp *qp, struct ibv_mw *mw,
                       struct ibv_mw_bind *mw_bind)
{
    int ret;
    struct ibv_mw_bind_info *bind_info = NULL;
    struct ibv_send_wr *bad_wr = NULL;
    struct ibv_send_wr wr = {};
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(qp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(mw);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(mw_bind);

    bind_info = &mw_bind->bind_info;
    if ((bind_info == NULL) || (bind_info->mr == NULL) || (mw->pd != qp->pd) || (mw->pd != bind_info->mr->pd)) {
        roce_err("para err, bind_info or bind_info->mr is NULL or mw->pd != qp->pd or mw->pd != bind_info->mr->pd");
        return -EINVAL;
    }

    if (mw->type == IBV_MW_TYPE_2) {
        roce_err("mw type is IBV_MW_TYPE_2");
        return -EINVAL;
    }

    if (!bind_info->mr && bind_info->length) {
        roce_err("para check err, bind_info->length is [%ld]", bind_info->length);
        return -EINVAL;
    }

    if (bind_info->mw_access_flags & IBV_ACCESS_ZERO_BASED) {
        roce_err("mw access flags check err, bind_info->mw_access_flags is %ld", bind_info->mw_access_flags);
        return -EINVAL;
    }

    wr.opcode = IBV_WR_BIND_MW;
    wr.next = NULL;
    wr.wr_id = mw_bind->wr_id;
    wr.send_flags = mw_bind->send_flags;
    wr.bind_mw.mw = mw;
    wr.bind_mw.rkey = ibv_inc_rkey(mw->rkey);
    wr.bind_mw.bind_info = mw_bind->bind_info;

    ret = hns_roce_u_v2_post_send(qp, &wr, &bad_wr);
    if (ret) {
        roce_err("post send failed ret [%d], expect 0", ret);
        return ret;
    }

    mw->rkey = wr.bind_mw.rkey;

    return 0;
}

struct ibv_mw *hns_roce_u_alloc_mw(struct ibv_pd *pd, enum ibv_mw_type type)
{
    struct ibv_mw *mw = NULL;
    struct ibv_alloc_mw cmd = {};
    struct ib_uverbs_alloc_mw_resp resp = {};
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(pd);

    mw = calloc(1, sizeof(struct ibv_mw));
    if (mw == NULL) {
        roce_err("calloc ibv_mv failed");
        return NULL;
    }

    if (ibv_cmd_alloc_mw(pd, type, mw, &cmd, sizeof(cmd), &resp, sizeof(resp))) {
        roce_err("ibv_cmd_alloc_mw failed");
        free(mw);
        mw = NULL;
        return NULL;
    }

    return mw;
}

int hns_roce_u_dealloc_mw(struct ibv_mw *mw)
{
    int ret;
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(mw);

    ret = ibv_cmd_dealloc_mw(mw);
    if (ret) {
        roce_err("ibv_cmd_dealloc_mw failed ret [%d], expect 0", ret);
        return ret;
    }

    free(mw);
    mw = NULL;
    return ret;
}

struct ibv_ah *hns_roce_u_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
    /*lint -e429*/
    struct hns_roce_ah *ah = NULL;
    int ret;
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(pd);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(attr);

    ah = calloc(1, sizeof(struct hns_roce_ah));
    if (ah == NULL) {
        roce_err("calloc ah failed");
        return NULL;
    }

    ah->av.port_pd = htole32(to_hr_pd(pd)->pdn | (attr->port_num << PORT_NUM_MAX));
    ah->av.sl = attr->sl;

    if (attr->static_rate) {
        ah->av.stat_rate = HNS_ROCE_STATIC_RATE;
    }

    if (attr->is_global) {
        ah->av.gid_index = attr->grh.sgid_index;
        ah->av.hop_limit = attr->grh.hop_limit;
        ah->av.tclass = attr->grh.traffic_class;
        ah->av.flowlabel = attr->grh.flow_label;
        ret = memcpy_s(ah->av.dgid, HNS_ROCE_GID_SIZE, attr->grh.dgid.raw, HNS_ROCE_GID_SIZE);
        if (ret) {
            roce_err("memcpy_s failed");
            free(ah);
            ah = NULL;
            return NULL;
        }
    }

    return &ah->ibv_ah;
    /*lint +e429*/
}

int hns_roce_u_destroy_ah(struct ibv_ah *ah)
{
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(ah);
    free(to_hr_ah(ah));
    ah = NULL;
    return 0;
}
