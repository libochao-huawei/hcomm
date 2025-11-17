/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hns_roce_u_cmd.h"
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#include "securec.h"
#ifndef HNS_ROCE_LLT
#include "dlog_pub.h"
#endif
#include "user_log.h"

#define ROCE_CDEV_FULL_PATH_MAX 32

#define ROCE_CLOSE_RETRY_FOR_EINTR(fd) do { \
    int ret_; \
    do { \
        ret_ = close(fd); \
        if (ret_ < 0) { \
            roce_warn("close filedscp[%d] failed, errno:%d", fd, errno); \
        } \
    } while ((ret_ < 0) && (errno == EINTR)); \
    fd = -1; \
} while (0)

static int roce_check_dev_name(const char *dev_name)
{
    size_t list_num, i;
    char err_char_list[] = {
        '.',
        '/',
    };

    if (strlen(dev_name) == 0) {
        roce_err("dev_name is invalid\n");
        return -EINVAL;
    }

    list_num = sizeof(err_char_list) / sizeof(err_char_list[0]);
    for (i = 0; i < list_num; i++) {
        if (strchr(dev_name, err_char_list[i]) != NULL) {
            return -EINVAL;
        }
    }

    return 0;
}

int roce_set_tsqp_depth(const char *dev_name, unsigned int rdev_index, unsigned int temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth)
{
    int ret;
    int fd = -1;
    char *real_path = NULL;
    char real_conf_path[PATH_MAX + 1] = {0};
    char file_path[ROCE_CDEV_FULL_PATH_MAX + 1] = "/dev/";
    struct roce_set_tsqp_depth_data tsqp_depth_data = {0};

    if (dev_name == NULL || qp_num == NULL || sq_depth == NULL) {
        roce_err("dev_name is NULL or qp_num or sq_depth is NULL\n");
        return -EINVAL;
    }

    if (roce_check_dev_name(dev_name) != 0) {
        roce_err("dev_name(%s) format is err\n", dev_name);
        return -EINVAL;
    }

    ret = strcat_s(file_path, ROCE_CDEV_FULL_PATH_MAX, dev_name);
    if (ret) {
        roce_err("Failed to strcat_s dev_name to file_path, ret[%d]", ret);
        return ret;
    }

    real_path = realpath(file_path, real_conf_path);
    if (real_path == NULL) {
        roce_err("file_path(%s) is invalid", file_path);
        return -EINVAL;
    }

    fd = open(real_path, O_RDONLY, S_IRUSR);
    if (fd < 0) {
        roce_err("Failed to open file_path[%s], err_code[%d]", file_path, errno);
        return -ENOENT;
    }

    tsqp_depth_data.rdev_index = rdev_index;
    tsqp_depth_data.temp_depth = temp_depth;
    ret = ioctl(fd, ROCE_CMD_SET_TSQP_DEPTH, &tsqp_depth_data);
    if (ret < 0) {
        roce_err("Failed to run ioctl, ret[%d], err_code[%d].", ret, errno);
        ROCE_CLOSE_RETRY_FOR_EINTR(fd);
        return ret;
    }

    ROCE_CLOSE_RETRY_FOR_EINTR(fd);
    *qp_num = tsqp_depth_data.qp_num;
    *sq_depth = tsqp_depth_data.sq_depth;
    return 0;
}

int roce_get_tsqp_depth(const char *dev_name, unsigned int rdev_index, unsigned int *temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth)
{
    int ret;
    int fd = -1;
    char *real_path = NULL;
    char real_conf_path[PATH_MAX + 1] = {0};
    char file_path[ROCE_CDEV_FULL_PATH_MAX + 1] = "/dev/";
    struct roce_get_tsqp_depth_data tsqp_depth_data = {0};

    if (dev_name == NULL || qp_num == NULL || temp_depth == NULL || sq_depth == NULL) {
        roce_err("dev_name is NULL or qp_num is NULL or temp_depth or sq_depth is NULL!\n");
        return -EINVAL;
    }

    if (roce_check_dev_name(dev_name) != 0) {
        roce_err("dev_name(%s) format is err\n", dev_name);
        return -EINVAL;
    }

    ret = strcat_s(file_path, ROCE_CDEV_FULL_PATH_MAX, dev_name);
    if (ret) {
        roce_err("Failed to strcat_s dev_name to file_path, ret[%d]", ret);
        return ret;
    }

    real_path = realpath(file_path, real_conf_path);
    if (real_path == NULL) {
        roce_err("file_path(%s) is invalid", file_path);
        return -EINVAL;
    }

    fd = open(real_path, O_RDONLY, S_IRUSR);
    if (fd < 0) {
        roce_err("Failed to open file_path[%s], err_code[%d]", real_path, errno);
        return -ENOENT;
    }

    tsqp_depth_data.rdev_index = rdev_index;
    ret = ioctl(fd, ROCE_CMD_GET_TSQP_DEPTH, &tsqp_depth_data);
    if (ret < 0) {
        roce_err("Failed to run ioctl, ret[%d], err_code[%d].", ret, errno);
        ROCE_CLOSE_RETRY_FOR_EINTR(fd);
        return ret;
    }

    ROCE_CLOSE_RETRY_FOR_EINTR(fd);
    *temp_depth = tsqp_depth_data.temp_depth;
    *qp_num = tsqp_depth_data.qp_num;
    *sq_depth = tsqp_depth_data.sq_depth;

    return 0;
}

int roce_get_roce_dev_data(const char *dev_name, struct roce_dev_data *dev_data)
{
    int ret;
    int fd = -1;
    char *real_path = NULL;
    char real_conf_path[PATH_MAX + 1] = {0};
    char file_path[ROCE_CDEV_FULL_PATH_MAX + 1] = "/dev/";

    if (dev_name == NULL || dev_data == NULL) {
        roce_err("dev_name is NULL or dev_data is NULL");
        return -EINVAL;
    }

    if (roce_check_dev_name(dev_name) != 0) {
        roce_err("dev_name(%s) format is err\n", dev_name);
        return -EINVAL;
    }

    ret = strcat_s(file_path, ROCE_CDEV_FULL_PATH_MAX, dev_name);
    if (ret) {
        roce_err("Failed to strcat_s dev_name to file_path, ret[%d]", ret);
        return ret;
    }

    real_path = realpath(file_path, real_conf_path);
    if (real_path == NULL) {
        roce_err("file_path(%s) is invalid", file_path);
        return -EINVAL;
    }

    fd = open(real_path, O_RDONLY, S_IRUSR);
    if (fd < 0) {
        roce_err("Failed to open file_path[%s], err_code[%d]", file_path, errno);
        return -ENOENT;
    }

    ret = ioctl(fd, ROCE_CMD_GET_ROCE_DEV_INFO, dev_data);
    if (ret < 0) {
        roce_err("Failed to run ioctl, ret[%d], err_code[%d].", ret, errno);
        ROCE_CLOSE_RETRY_FOR_EINTR(fd);
        return ret;
    }

    ROCE_CLOSE_RETRY_FOR_EINTR(fd);

    return 0;
}

int hns_roce_u_get_roce_qpc_stat(const struct ibv_context *ibv_ctx,
    struct hns_roce_qpc_stat *qpc_stat)
{
    char file_path[ROCE_CDEV_FULL_PATH_MAX + 1] = "/dev/";
    char real_conf_path[PATH_MAX + 1] = {0};
    char *real_path = NULL;
    char *dev_name = NULL;
    int fd = -1;
    int ret;

    if (ibv_ctx == NULL || qpc_stat == NULL || ibv_ctx->device == NULL) {
        roce_err("ibv_ctx is NULL or qpc_stat or ibv_ctx->device is NULL");
        return -EINVAL;
    }

    dev_name = ibv_ctx->device->name;
    if (dev_name == NULL) {
        roce_err("dev_name is NULL or dev_data is NULL");
        return -EINVAL;
    }

    if (roce_check_dev_name(dev_name) != 0) {
        roce_err("dev_name(%s) format is err\n", dev_name);
        return -EINVAL;
    }

    ret = strcat_s(file_path, ROCE_CDEV_FULL_PATH_MAX, dev_name);
    if (ret) {
        roce_err("Failed to strcat_s dev_name to file_path, ret[%d]", ret);
        return ret;
    }

    real_path = realpath(file_path, real_conf_path);
    if (real_path == NULL) {
        roce_err("file_path(%s) is invalid", file_path);
        return -EINVAL;
    }

    fd = open(real_path, O_RDONLY, S_IRUSR);
    if (fd < 0) {
        roce_err("Failed to open file_path[%s], err_code[%d]", file_path, errno);
        return -ENOENT;
    }

    ret = ioctl(fd, ROCE_CMD_GET_ROCE_QPC_STAT, qpc_stat);
    if (ret < 0) {
        roce_err("Failed to run ioctl, ret[%d], err_code[%d].", ret, errno);
        ROCE_CLOSE_RETRY_FOR_EINTR(fd);
        return ret;
    }

    ROCE_CLOSE_RETRY_FOR_EINTR(fd);

    return 0;
}

int roce_query_qpc(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attr_val, unsigned int attr_mask)
{
    roce_info("roce query qpc is not support.");

    return 0;
}

int roce_mmap_ai_db_reg(struct ibv_context *ibv_ctx, unsigned int tgid)
{
    roce_info("roce mmap ai db reg is not support.");

    return 0;
}

int roce_unmmap_ai_db_reg(struct ibv_context *ibv_ctx)
{
    roce_info("roce unmmap ai db reg is not support.");

    return 0;
}

int roce_get_cq_data_plane_info(struct ibv_cq *cq, struct hns_roce_cq_data_plane_info *info)
{
    roce_info("roce get cq data plane info is not support.");

    return 0;
}

int roce_get_qp_data_plane_info(struct ibv_qp *qp, struct hns_roce_qp_data_plane_info *info)
{
    roce_info("roce get qp data plane info is not support.");

    return 0;
}

int roce_remap_mr(struct ibv_mr *ibvmr, struct hns_roce_mr_remap_info info[], unsigned int num)
{
    roce_info("roce remap mr is not support.");

    return 0;
}

unsigned int roce_get_api_version(void)
{
    return ROCE_API_VERSION;
}
