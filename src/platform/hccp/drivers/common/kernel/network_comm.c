/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: driver common function realization
 * Create: 2025-10-25
 */

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/kthread.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/mmap_lock.h>
#include <linux/sched/mm.h>
#include <linux/semaphore.h>
#include <linux/version.h>
#include "network_comm.h"

#ifndef CONFIG_LLT
STATIC char *get_cur_proc_path(const char *module_name, const char **p_file_name, char *file_path_in, u32 path_len)
{
    struct file *exe_file = NULL;
    char *path_str = NULL;

    /* reference to get_mm_exe_file */
    rcu_read_lock();
    exe_file = rcu_dereference(current->mm->exe_file);
    if (exe_file == NULL) {
        rcu_read_unlock();
        network_drv_err(module_name, "cannot find exe file.\n");
        return NULL;
    }

    *p_file_name = exe_file->f_path.dentry->d_name.name;
    path_str = d_path(&exe_file->f_path, file_path_in, path_len);
    if (IS_ERR_OR_NULL(path_str)) {
        network_drv_err(module_name, "path_str is err or null.(errno=%ld)\n", PTR_ERR(path_str));
        path_str = NULL;
    }
    rcu_read_unlock();

    return path_str;
}
#endif

int verify_process_whitelist(const char *module_name, const white_list_item_t *white_list, size_t list_size)
{
    const char *cfg_proc_name = NULL;
    const char *proc_name = NULL;
    char *cur_path = NULL;
    char *path_str = NULL;
    int ret = -EINVAL;
    unsigned int i;
    uid_t uid;

#ifndef CONFIG_LLT
    path_str = (char *)vzalloc(PATH_MAX);
    DRV_CHK_PRT_RETURN(path_str == NULL, network_drv_err(module_name, "path_str alloc failed\n"), -ENOMEM);

    uid = __kuid_val(current_uid());
    cur_path = get_cur_proc_path(module_name, &proc_name, path_str, PATH_MAX);
    if (cur_path == NULL) {
        network_drv_err(module_name, "get_cur_proc_path failed\n");
        goto path_str_free;
    }

    network_drv_info(module_name, "process info: proc name=\"%s\"; path=\"%s\"; uid=%u\n", proc_name, cur_path, uid);

    for (i = 0; i < list_size; i++) {
        if (white_list[i].uid != uid) {
            continue;
        }

        cfg_proc_name = kbasename(white_list[i].path);
        if (cfg_proc_name == NULL) {
            continue;
        }
        if (strcmp(cfg_proc_name, proc_name) == 0 && (strcmp(white_list[i].path, cur_path) == 0)) {
            ret = 0;
            goto path_str_free;
        }
    }
    ret = -EINVAL;
    network_drv_err(module_name, "Whitelist matching failed, proc name=\"%s\"; path=\"%s\"; uid=%u\n",
        proc_name, cur_path, uid);

path_str_free:
    vfree(path_str);
    path_str = NULL;
#endif
    return ret;
}
