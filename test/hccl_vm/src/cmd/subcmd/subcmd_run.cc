/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string>

#include "cmd_base_utils.h"
#include "sim_common_defs.h"
#include "sim_log.h"
#include "subcmd_run.h"

namespace HcclSim {
void RunCommand::Setup(CLI::App& app) {
    auto sub_oneShot = app.add_subcommand("run", "算例one_shot运行模式, 请勿在子bash中重复启用,命令后必须接有算例执行指令");

    sub_oneShot->add_option("configFile", configClusterName, "加载昇腾集群组网配置目录")->required()->check(GenerateClusterTopo);
    sub_oneShot->add_option("--level", g_hcclVmLevel, "设置模拟等级, 当前支持等级为 1 和 2, 默认模拟等级 2 ");
    sub_oneShot->allow_extras(true);
    
    sub_oneShot->callback([this, &app]() { Execute(app); });
}

void RunCommand::Execute(CLI::App& app) {
    if (g_hcclVmBashFlag) {
        HCCL_VM_WARN("[HVM] hccl-vm is already running. Please do not run examples in one_shot mode within a sub-bash. Exit the sub-bash and try again.");
        return;
    }
    CLI::App* tmp_cmd = app.get_subcommand("run");
    std::vector<std::string> leftargvs = tmp_cmd->remaining();
    if (leftargvs.empty()) {
        HCCL_VM_ERROR("[HVM] In one_shot mode, an example execution command must be provided.");
        return;
    }
    HCCL_VM_INFO("[HVM] Initializing: Model={}, Level={}", configClusterName, g_hcclVmLevel);
    auto clusterDir = GetBinLocation() + "/cluster_model/network/cluster/" + configClusterName;
    auto ret = InitHvmEnv(clusterDir, g_hcclVmLevel);
    if (ret != HcclVmResult::HCCL_SIM_HOST_SUCCESS_CMD) {
        HCCL_VM_ERROR("[HVM] Failed to initialize simulation environment. Cleaning up environment.");
        auto cleanRet = HcclVmExit();
        if (cleanRet != HcclVmResult::HCCL_SIM_HOST_SUCCESS_CMD) {
            HCCL_VM_ERROR("[HVM] Failed to clean up environment. Please check for residual environment artifacts.");
        }
        return;
    }
    CstyleCmd syscmd(leftargvs);
    HCCL_VM_INFO("[HVM] one_shot mode, executing: {}", syscmd.cmd());
    std::string proxyPath = GetBinLocation() + "/libhccl_proxy_level2.so";
    setenv("LD_PRELOAD", proxyPath.c_str(), 1);
    int sysRet = std::system(syscmd.cmd().c_str()); // system() 会阻塞当前进程直到子命令结束
    if (sysRet != 0) {
        HCCL_VM_ERROR("[HVM] Example execution failed: {}", sysRet);
    }
    RemoveFromLDPreload(proxyPath);
    auto cleanRet = HcclVmExit();
    return;
}

static inline CommandAutoRegister<RunCommand> g_run_cmd_reg{};
}
