/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "network_comm.h"
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include "securec.h"
#include "user_log.h"

#define ROOT_PATH "/root"

int net_comm_get_self_home(char *home_path, unsigned int path_len)
{
    int ret, ret_val;
    struct passwd *pwd = getpwuid(getuid());
    CHK_PRT_RETURN(pwd == NULL, roce_err("pwd is NULL! getpwuid fail, errno:%d", errno), -EINVAL);

    if (pwd->pw_name == NULL) {
        roce_err("pwd->pw_name is NULL, errno:%d", errno);
        ret = -EINVAL;
        goto out_get_self_home;
    }

    // root用户的home路径为/root
    // 其他用户的home路径为/home/${user_name}
    if (strncmp(pwd->pw_name, "root", strlen("root") + 1) == 0) {
        ret = sprintf_s((char *)home_path, path_len, ROOT_PATH);
    } else {
        ret = sprintf_s((char *)home_path, path_len, "/home/%s", pwd->pw_name);
    }
    if (ret <= 0) {
        roce_err("sprintf_s for user name:%s failed, ret:%d ", pwd->pw_name, ret);
        ret = -ENOMEM;
        goto out_get_self_home;
    }

    ret = 0;
out_get_self_home:
    ret_val = memset_s(pwd, sizeof(struct passwd), 0, sizeof(struct passwd));
    if (ret_val) {
        roce_err("memset error, ret_val[%d]", ret_val);
        ret = ret_val;
    }

    return ret;
}

int net_get_gateway_address(unsigned int phy_id, unsigned int *gtw_addr)
{
    unsigned int gtw_dst = NET_INVALID_GW;
    unsigned int eth_id = 0;
    char buf[BUF_LEN];
    int ret, ret_val;
    char *tmp = NULL;
    FILE *fp = NULL;

    fp = fopen("/proc/net/route", "r");
    if (fp == NULL) {
        roce_err("fopen failed, errno:%d", errno);
        return -EINVAL;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        tmp = buf;
        while (tmp != NULL && (*tmp == ' ')) {
            ++tmp;
        }

        if (tmp == NULL || (strncmp(tmp, "eth", strlen("eth")) != 0)) {
            continue;
        }

        ret = sscanf_s(tmp, "eth%u%x%x", &eth_id, &gtw_dst, gtw_addr);
        if (ret != NET_THREE_VALUE) {
            roce_err("sscanf buf(%s) to gtw_addr failed. ret(%d)", buf, ret);
            ret = -ENOMEM;
            goto out;
        }

        if (gtw_dst == 0 && eth_id == phy_id) {
            ret = 0;
            goto out;
        }
    }

    roce_err("cannot find gateway by phy_id: %u", phy_id);
    ret = -ENOENT;
out:
    ret_val = fclose(fp);
    if (ret_val) {
        roce_warn("fclose fail, ret_val:%d, errno:%d", ret_val, errno);
    }
    fp = NULL;
    return ret;
}
