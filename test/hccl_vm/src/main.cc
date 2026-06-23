/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdio>
#include <cstdlib>
#include <string>

#include "cmd_base.h"
#include "cmd_base_utils.h"
#include "sim_log.h"
#include "db_sim_op_db_ops.h"

void envInit()
{
    int ret3 = system("sudo rm -fr ./hccl_vm_data.db ./hccl_vm_data.db-wal ./hccl_vm_data.db-shm 2>/dev/null");
    int ret1 = system("sudo rm -fr /dev/shm/* 2>/dev/null");
    int ret2 = system("sudo rm -fr /tmp/hccl_sim.db* 2>/dev/null");
    if (ret1 != 0 || ret2 != 0) {
        HCCL_VM_ERROR("envInit failed\n");
    }
    sim::InitOpDataDb();
    HCCL_VM_INFO("envInit success");
}

int main(int argc, char *argv[])
{
    LogConfig config;
    config.fileBaseName = "hccl_vm";
    InitLogger(config);

    const char* envCheck = std::getenv(HVM_BASH_ENV_KEY.c_str());

    if (envCheck != nullptr) {
        StartHostClient(argc, argv);
    } else {
        envInit();
        std::string cmd = ArgvToString(argc, argv);
        if (argc == 1) {
            cmd += " --help";
        }
        ParseCommand(cmd);
    }
    return 0;
}
