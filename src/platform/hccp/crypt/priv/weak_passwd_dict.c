/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "weak_passwd_dict.h"
#include <errno.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include "securec.h"
#include "user_log.h"
#include "tls.h"
#include "ossl_user_linux.h"
#include "hccn_comm.h"
#include "file_opt.h"

#define ROOT_CONFIG_WEAK_DICT "/etc/hccn_weak_dict.conf"
#define WEAK_PWD_MAX_LEN 1024
#define WEAK_PWD_MAX_NUMS 10000
#define WEAK_PWD_PATH_LEN 64

#define WEAK_PWD_FILE_MAX_LEN (WEAK_PWD_MAX_LEN * WEAK_PWD_MAX_NUMS)

STATIC void crypto_filter_str(char *str, unsigned int *len, char filter)
{
    int index;

    if (*len == 0) {
        return;
    }

    for (index = *len - 1; index >= 0; index--) {
        if (str[index] == filter) {
            str[index] = '\0';
            break;
        }
    }

    *len = strlen(str);
}

STATIC int crypto_check_weak_pwd_inner(char *weak_pwd_buf, unsigned int buf_len, FILE *fp, const char *pwd)
{
    int ret;
    unsigned int weak_pwd_cnt = 0;
    unsigned int weak_pwd_len;

    while (fgets(weak_pwd_buf, buf_len, fp) != NULL) {
        weak_pwd_cnt++;
        if (weak_pwd_cnt > WEAK_PWD_MAX_NUMS) {
            return UDA_PARAM_TLS_WEAK_PWD_DICT_TOO_MANY;
        }

        weak_pwd_len = strlen(weak_pwd_buf);
        if (weak_pwd_len > WEAK_PWD_MAX_LEN) {
            roce_err("weak_pwd_len[%u] is too long, invalid", weak_pwd_len);
            return UDA_TOOL_INNER_PARAM_ERR;
        }

        crypto_filter_str(weak_pwd_buf, &weak_pwd_len, '\n');
        if (weak_pwd_len > PWD_MAX_LEN) {
            continue;
        }

        if (weak_pwd_len == strlen(pwd) && strncmp(weak_pwd_buf, pwd, strlen(pwd)) == 0) {
            return UDA_PARAM_TLS_PWD_TOO_WEAK_ERR;
        }

        ret = memset_s(weak_pwd_buf, buf_len, 0, buf_len);
        if (ret) {
            roce_err("memset_s for weak_pwd_buf failed, ret:%d", ret);
            return UDA_TOOL_INNER_PARAM_ERR;
        }
    }

    if (weak_pwd_cnt == 0) {
        return UDA_PARAM_TLS_WEAK_PWD_DICT_NULL;
    }

    return 0;
}

STATIC int crypto_get_weak_pwd_file(const char *weak_pwd_path)
{
    int ret;
    char *buf = NULL;
    unsigned int buf_size = WEAK_PWD_FILE_MAX_LEN;

    buf = calloc(1, WEAK_PWD_FILE_MAX_LEN);
    if (buf == NULL) {
        roce_err("buf is NULL! no memory");
        return -ENOMEM;
    }

    ret = read_file_to_buf(ROOT_CONFIG_WEAK_DICT, buf, &buf_size);
    if (ret == -ENOENT) {
        goto out;
    }

    if (ret) {
        roce_err("read_file_to_buf for [%s] fail, ret:%d", ROOT_CONFIG_WEAK_DICT, ret);
        goto out;
    }

    ret = write_buf_to_file((const unsigned char*)buf, buf_size, weak_pwd_path, S_IRUSR | S_IWUSR);
    if (ret) {
        roce_err("write_buf_to_file for [%s] fail, ret:%d", weak_pwd_path, ret);
        goto out;
    }

    ret = 0;
out:
    free(buf);
    return ret;
}

STATIC int crypto_check_weak_pwd_for_non_root(const char *weak_pwd_path, const char *pwd)
{
    int ret, ret_val;
    char weak_pwd[WEAK_PWD_MAX_LEN + 1] = {0};
    char real_conf_path[PATH_MAX + 1] = {0};//lint !e813
    FILE *fp = NULL;

    if (strlen(weak_pwd_path) > PATH_MAX) {
        roce_err("path_len[%lu] > [%d](PATH_MAX)", strlen(weak_pwd_path), PATH_MAX);
        return -EINVAL;
    }

    if (realpath(weak_pwd_path, real_conf_path) == NULL) {
        ret = -errno;
        if (ret == -ENOENT) {
            ret = crypto_get_weak_pwd_file(real_conf_path);
            if (ret == -ENOENT) { // if root alse has no weak_pwd_dict, it is normal;
                return ret;
            }
            if (ret) {
                roce_err("crypto_get_weak_pwd_for_non_root failed, ret:%d", ret);
                return ret;
            }
        } else {
            roce_err("realpath for weak_pwd_path[%s] failed, ret:%d", weak_pwd_path, ret);
            return ret;
        }
    }

    fp = fopen(real_conf_path, "r");
    if (fp == NULL) {
        roce_err("fp is NULL, fail to read weak dictionary");
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    ret = crypto_check_weak_pwd_inner(weak_pwd, WEAK_PWD_MAX_LEN, fp, pwd);
    if (ret) {
        roce_err("crypto_check_weak_pwd_inner failed, ret:%d", ret);
    }

    ret_val = fclose(fp);
    fp = NULL;
    if (ret_val) {
        roce_err("fclose fail, ret_val:%d, err:%d", ret_val, errno);
        ret = ret_val;
    }

    return ret;
}

STATIC int crypto_check_weak_pwd_for_root(const char *weak_pwd_path, const char *pwd)
{
    int ret, ret_val;
    char weak_pwd[WEAK_PWD_MAX_LEN + 1] = {0};
    char real_conf_path[PATH_MAX + 1] = {0};//lint !e813
    FILE *fp = NULL;

    if (strlen(weak_pwd_path) > PATH_MAX) {
        roce_err("path_len[%lu] > [%d](PATH_MAX)", strlen(weak_pwd_path), PATH_MAX);
        return -EINVAL;
    }

    if (realpath(weak_pwd_path, real_conf_path) == NULL) {
        ret = -errno;
        if (ret == -ENOENT) {
            return UDA_PARAM_TLS_NO_WEAK_PWD_DICT;
        }
        roce_err("realpath for weak_pwd_path[%s] failed, ret:%d", weak_pwd_path, ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    fp = fopen(real_conf_path, "r");
    if (fp == NULL) {
        roce_err("fp is NULL, fail to read weak dictionary");
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    ret = crypto_check_weak_pwd_inner(weak_pwd, WEAK_PWD_MAX_LEN, fp, pwd);
    if (ret) {
        roce_err("crypto_check_weak_pwd_inner failed, ret:%d", ret);
    }

    ret_val = fclose(fp);
    fp = NULL;
    if (ret_val) {
        roce_err("fclose fail, ret_val:%d, errno:%d", ret_val, errno);
        ret = ret_val;
    }

    return ret;
}

int crypto_check_weak_pwd(const char *pwd, int pwd_len)
{
    int ret;
    char weak_pwd_path[WEAK_PWD_PATH_LEN + 1] = {0};

    if (pwd == NULL || pwd_len <= 0) {
        roce_err("pwd is NULL or pwd_len[%d] <= 0, invalid", pwd_len);
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    if (strncmp(hccn_get_g_usr_name(), "root", strlen("root") + 1) == 0) {
        ret = crypto_check_weak_pwd_for_root(ROOT_CONFIG_WEAK_DICT, pwd);
        if (ret) {
            if (ret != UDA_PARAM_TLS_NO_WEAK_PWD_DICT) {
                roce_err("crypto_check_weak_pwd_for_root failed, ret:%d", ret);
            }
            return ret;
        }
    } else {
        ret = sprintf_s(weak_pwd_path, WEAK_PWD_PATH_LEN, "/home/%s/hccn_weak_dict.conf", hccn_get_g_usr_name());
        if (ret <= 0) {
            roce_err("sprintf_s for weak_pwd_path failed, ret:%d usr_name:%s", ret, hccn_get_g_usr_name());
            return -ENOMEM;
        }

        ret = crypto_check_weak_pwd_for_non_root(weak_pwd_path, pwd);
        if (ret) {
            if (ret == -ENOENT) {
                return UDA_PARAM_TLS_NO_WEAK_PWD_DICT;
            }
            roce_err("crypto_check_weak_pwd_for_non_root failed, ret:%d", ret);
            return ret;
        }
    }

    return 0;
}
