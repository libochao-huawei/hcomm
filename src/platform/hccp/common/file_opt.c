/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "file_opt.h"
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include "securec.h"
#include "user_log.h"

#ifndef CRYPT_LLT
#define STATIC static
#else
#define STATIC
#endif

STATIC int read_file_to_buf_check_param(const char *path, const char *content, const unsigned int *content_len)
{
    if (path == NULL || content == NULL || content_len == NULL) {
        roce_err("path or conf_path or content_len is NULL, invalid");
        return -EINVAL;
    }

    if (strlen(path) > PATH_MAX) {
        roce_err("path_len[%lu] > [%d](PATH_MAX)", strlen(path), PATH_MAX);
        return -EFAULT;
    }

    return 0;
}

int read_file_to_buf(const char *path, char *content, unsigned int *content_len)
{
    size_t len;
    long temp_len;
    int ret, ret_val;
    FILE *read_file = NULL;
    char real_conf_path[PATH_MAX + 1] = {0};//lint !e813

    ret = read_file_to_buf_check_param(path, content, content_len);
    if (ret) {
        roce_err("read_file_to_buf_check_param failed, ret[%d]", ret);
        return ret;
    }

    if (realpath(path, real_conf_path) == NULL) {
        ret = -errno;
        if (ret != -ENOENT) {
            roce_err("conf_path[%s] is invalid, err[%d]", path, ret);
        }
        return ret;
    }

    read_file = fopen(real_conf_path, "r");
    if (read_file == NULL) {
        roce_err("read_file is NULL, invalid");
        return -EINVAL;
    }

    ret = fseek(read_file, 0, SEEK_END);
    if (ret < 0) {
        roce_err("fseek failed with error:%d", errno);
        goto out;
    }

    temp_len = ftell(read_file);
    if (temp_len <= 0 || temp_len > (int)*content_len || temp_len > INT_MAX) {
        ret = -EINVAL;
        roce_err("ftell failed with error:%d, temp_len=%ld, content_len[%u]", errno, temp_len, *content_len);
        goto out;
    }

    len = (size_t)temp_len;

    rewind(read_file);
    ret = (int)fread((void *)content, len, 1, read_file); /* read a buf which size is len */
    if (ret != 1) {
        roce_err("fread failed ret:%d, error:%d, ferror(fp):%d", ret, errno, ferror(read_file));
        ret = -EINVAL;
        goto out;
    }
    ret = 0;
    *content_len = (unsigned int)len;
out:
    ret_val = fclose(read_file);
    if (ret_val) {
        roce_warn("fclose fail, ret_val:%d, errno:%d", ret_val, errno);
    }

    return ret;
}

STATIC void check_file(const char *file)
{
    int ret;

    if (access(file, F_OK) == FILE_EXIST) {
        ret = remove(file);
        roce_run_info("file %s exist, should remove it, ret:%d, errno:%d", file, ret, errno);
    } else {
        ret = errno;
        if (ret != ENOENT) {
            roce_run_info("access file %s invalid, errno:%d", file, ret);
        }
    }
}

int write_buf_to_file(const unsigned char *buf, unsigned int buf_len, const char *file, mode_t mode)
{
    int ret, ret_val;
    int fd = -1;

    if (buf == NULL || file == NULL) {
        roce_err("buf or file is NULL, invalid!");
        return -EINVAL;
    }

    check_file(file);
    fd = creat(file, mode);
    if (fd < 0) {
        ret = -errno;
        roce_err("create file %s failed errno %d", file, ret);
        return ret;
    }

    ret = (int)write(fd, buf, buf_len);
    if (ret != (int)buf_len) {
        roce_err("write file %s failed %d, buf_len:%u", file, ret, buf_len);
        ret = -errno;
        do {
            ret_val = close(fd);
        } while ((ret_val < 0) && (errno == EINTR));
        remove_file(file);
        return ret;
    } else {
        ret = 0;
    }

    do {
        ret_val = close(fd);
    } while ((ret_val < 0) && (errno == EINTR));

    fd = -1;

    ret = chmod(file, mode);
    if (ret != 0) {
        roce_err("file[%s] chmod fail, errno: %d.", file, ret);
        remove_file(file);
    }
    return ret;
}

void remove_file(const char *file)
{
    int ret, err;

    if (file == NULL) {
        roce_err("file is NULL, invalid");
        return;
    }

    ret = remove(file);
    if (ret < 0) {
        err = -errno;
        if (err != -ENOENT) {
            roce_err("remove end file failed %d", err);
            return;
        }
    }
    return;
}

int check_file_path(const char *path, mode_t mode)
{
    int ret;
    char real_conf_path[PATH_MAX + 1] = {0};//lint !e813

    if (path == NULL) {
        roce_err("path is NULL");
        return -EINVAL;
    }

    if (strlen(path) > PATH_MAX) {
        roce_err("path_len[%lu] > [%d](PATH_MAX)", strlen(path), PATH_MAX);
        return -EINVAL;
    }

    if (realpath(path, real_conf_path) == NULL) {
        ret = -errno;
        if (ret == -ENOENT) {
            roce_warn("path[%s] is not exist, real_path[%s]", path, real_conf_path);
            ret = mkdir(real_conf_path, mode);
            if (ret) {
                roce_err("mkdir real_conf_path[%s] failed, ret[%d]", real_conf_path, ret);
                return ret;
            }
            ret = chmod(real_conf_path, mode);
            if (ret != 0) {
                roce_err("file[%s] chmod fail, errno: %d.", real_conf_path, ret);
            }
            return ret;
        }

        roce_err("path[%s] is invalid, err[%d]", path, ret);
        return ret;
    }

    return 0;
}

static void close_fd_security(int fd)
{
    int ret;
    int err_no = -1;

    do {
        ret = close(fd);
        if (ret < 0) {
            err_no = errno;
            if (err_no != EINTR) {
                roce_err("close fd[%d] failed, ret:%d, err_no[%d]", fd, ret, err_no);
                return;
            }
        }
    } while ((ret < 0) && (err_no == EINTR));

    return;
}

int get_file_lock(const char *path, int *lock_fd)
{
    int ret, err;
    int fd = -1;

    if (lock_fd == NULL || path == NULL) {
        roce_err("lock_fd is NULL or path is NULL.");
        return -EINVAL;
    }

    if (strlen(path) > PATH_MAX) {
        roce_err("path_len[%lu] > [%d](PATH_MAX)", strlen(path), PATH_MAX);
        return -EINVAL;
    }

    fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        ret = -errno;
        roce_err("open real_path[%s] failed, ret[%d]", path, ret);
        return ret;
    }

    ret = flock(fd, LOCK_EX | LOCK_NB);
    if (ret) {
        err = -errno;
        close_fd_security(fd);
        if (err == -EWOULDBLOCK) {
            return -EBUSY;
        }
        roce_err("lock fd[%d] failed, err[%d]", fd, err);
        return err;
    }

    *lock_fd = fd;
    return 0;
}

int release_file_lock(int lock_fd, const char *lock_file_path)
{
    int ret, err;

    if (lock_fd < 0) {
        roce_err("invalid lock_fd[%d]", lock_fd);
        return -EINVAL;
    }

    ret = flock(lock_fd, LOCK_UN);
    if (ret) {
        err = -errno;
        roce_err("release lock fd[%d] failed, err[%d]", lock_fd, err);
        close_fd_security(lock_fd);
        return err;
    }

    close_fd_security(lock_fd);

    remove_file(lock_file_path);
    return 0;
}

STATIC int cfg_inner_read_conf_byfd(FILE *fp, const char *conf_name, char *conf_value, unsigned int len)
{
    int ret = FILE_OPT_SYS_RD_FILE_NOT_FOUND;
    char *line_buf = NULL;
    unsigned long len_buf;
    unsigned int len_tmp;
    char *c = NULL;

    line_buf = calloc(CONLINE_LEN, sizeof(char));
    if (line_buf == NULL) {
        roce_err("calloc line_buf failed");
        return FILE_OPT_NO_MEM_ERR;
    }

    while (feof(fp) == 0 && fgets(line_buf, CONLINE_LEN, fp) != NULL) {
        if ((line_buf[0] == '#') || (strlen(line_buf) < strlen("*=*"))) {
            continue;
        }
        c = (char *)strchr(line_buf, '=');
        if (c == NULL) {
            continue;
        }

        len_buf = strlen(line_buf) - 1;
        if ((len_buf < CONLINE_LEN) && (line_buf[len_buf] == '\n')) {
            line_buf[len_buf] = '\0';
        }

        len_tmp = (unsigned int)(c - line_buf);
        if ((strncmp(line_buf, conf_name, strlen(conf_name)) == 0) && (len_tmp == strlen(conf_name))) {
            ++c;
            ret = strcpy_s(conf_value, len, c);
            if (ret) {
                roce_err("strcpy_s err[%d], len:%u", ret, len);
                ret = FILE_OPT_NO_MEM_ERR;
            }

            goto out;
        }
    }

out:
    free(line_buf);
    line_buf = NULL;
    return ret;
}

STATIC int cfg_inner_read_conf(const char *conf_path, const char *conf_name, char *conf_value, unsigned int len)
{
    char real_conf_path[PATH_MAX + 1] = {0};//lint !e813
    int ret, ret_val;
    FILE *fp = NULL;

    // file not exist, degrade log level
    if ((strlen(conf_path) > PATH_MAX) || (realpath(conf_path, real_conf_path) == NULL)) {
        roce_warn("read path_len[%u] > PATH_MAX[%d] or conf_path is invalid, errno[%d]",
            strlen(conf_path), PATH_MAX, errno);
        return FILE_OPT_INNER_PARAM_ERR;
    }

    fp = fopen(real_conf_path, "r");
    if (fp == NULL) {
        roce_err("Open configure file fail errno[%d] real_conf_path[%s]", errno, real_conf_path);
        return FILE_OPT_SYS_READ_FILE_ERR;
    }

    ret = flock(fileno(fp), LOCK_EX);
    if (ret) {
        roce_err("hccn.conf lock fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
        ret = FILE_OPT_SYS_READ_FILE_ERR;
        goto out;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret) {
        roce_err("hccn.conf fseek fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
        ret = FILE_OPT_SYS_READ_FILE_ERR;
        goto out;
    }

    ret = cfg_inner_read_conf_byfd(fp, conf_name, conf_value, len);
out:
    ret_val = fclose(fp);
    FILE_CHECK_RET_WITHOUT_RETURN(ret_val, "fclose fail, ret_val:%d, errno:%d", ret_val, errno);
    fp = NULL;
    return ret;
}

int file_read_cfg(const char *file_path, int dev_id, const char *conf_name, char *conf_value, unsigned int len)
{
    char conf[CONLINE_LEN] = {0};
    int ret;

    FILE_CHECK_PTR_VALID_RETURN_VAL(file_path, -EINVAL);
    FILE_CHECK_PTR_VALID_RETURN_VAL(conf_name, -EINVAL);
    FILE_CHECK_PTR_VALID_RETURN_VAL(conf_value, -EINVAL);

    ret = sprintf_s(conf, CONLINE_LEN, "%s_%d", conf_name, dev_id);
    if (ret <= 0) {
        roce_err("conf str op fail! ret[%d] conf_name[%s] dev_id[%d]", ret, conf_name, dev_id);
        return -EINVAL;
    }

    ret = cfg_inner_read_conf(file_path, conf, conf_value, len);
    if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND) {
        roce_info("the conf[%s] not found", conf);
        return ret;
    } else if (ret == FILE_OPT_INNER_PARAM_ERR) {
        roce_warn("cfg_inner_read_conf unsuccessful ret[%d]", ret);
        return ret;
    } else if (ret != 0) {
        roce_err("cfg_inner_read_conf fail! ret[%d] conf[%s] conf_value[%s]", ret, conf, conf_value);
        return ret;
    }

    roce_info("read conf[%s] realconf[%s] val[%s]", conf, conf_name, conf_value);

    return 0;
}
