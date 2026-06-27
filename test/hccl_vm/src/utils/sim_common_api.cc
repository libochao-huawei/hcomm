/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "sim_common_api.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>
#include <climits>

static std::string GetExePath()
{
    char buf[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return "";
    buf[len] = '\0';
    return std::string(buf);
}

static std::string GetExeDir()
{
    std::string full = GetExePath();
    if (full.empty()) {
        return ".";
    }
    size_t pos = full.find_last_of('/');
    return (pos == std::string::npos) ? "." : full.substr(0, pos);
}

static std::string ComputeInstallRoot()
{
    const char* env = std::getenv("HCCL_VM_INSTALL_ROOT");
    if (env && *env) {
        return std::string(env);
    }
    std::string exe = GetExePath();
    if (exe.empty()) {
        return ".";
    }

    std::string cur = exe;
    for (int i = 0; i < 8; ++i) {
        size_t pos = cur.find_last_of('/');
        if (pos == std::string::npos || pos == 0) {
            break;
        }
        cur = cur.substr(0, pos);
        if (std::ifstream(cur + "/config/log_config.yaml").good()) {
            return cur;
        }
    }
    return GetExeDir();
}

const std::string& InstallPath::GetHcclVmInstallAbsPath()
{
    static const std::string root = ComputeInstallRoot();
    return root;
}

std::string InstallPath::ResolveToInstallRoot(const std::string& relPath)
{
    if (relPath.empty() || relPath[0] == '/' ||
        (relPath.size() >= 2 && relPath[0] == '.' && relPath[1] == '/')) {
        return relPath;
    }
    return GetHcclVmInstallAbsPath() + "/" + relPath;
}
