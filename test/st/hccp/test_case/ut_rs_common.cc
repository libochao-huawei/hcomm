/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include "hccp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <libgen.h>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <signal.h>
#include <cstring>
#include <cstdlib>
#include "ascend_hal.h"
#include "ascend_hal_stub.h"
using namespace std;


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#ifdef __cplusplus
}
#endif // __cplusplus

class HccpSystemTest : public ::testing::Test {
protected:
    std::map<int, pid_t> host_pids_;
    using HostTestLogic = std::function<void(int phyid)>;
    std::map<int, HostTestLogic> host_logic_map_;
    pid_t main_host_pid_ = -1;
    int port_ = 0;
    std::vector<int> phys_ids_;

    void PrintExitStatus(int status) {
        if (WIFEXITED(status)) {
            std::cout << "code=" << WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            std::cout << "signal=" << sig;
            if (sig == SIGSEGV) std::cout << " (SIGSEGV)";
            else if (sig == SIGUSR1) std::cout << " (SIGUSR1)";
        }
        std::cout << "\n";
    }

    void InitHalStubForHost() {
        setHostPid(getpid());
        setDevPid(0);
        InitHdcId();
    }

    void CleanupDeviceProcess(int phy_id, pid_t dev_pid) {
        if (dev_pid == -1) return;

        int status = 0;
        std::cout << "[HOST-" << phy_id << "] Cleaning up device (pid=" << dev_pid << ")\n";
        
        kill(dev_pid, SIGUSR1);
        pid_t result = waitpid(dev_pid, &status, 0);
        
        if (result == -1) {
            std::cerr << "[HOST-" << phy_id << "] waitpid device failed\n";
        } else {
            std::cout << "[HOST-" << phy_id << "] Device exited: ";
            PrintExitStatus(status);
        }
    }

public:
    pid_t SpawnDeviceProcess(int phy_id) {
        pid_t corresponding_host_pid = getpid(); // 直接获取当前 Host 进程的 PID

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "[HOST-" << phy_id << "] Fork device failed" << std::endl;
            return -1;
        }

        if (pid == 0) {
            // ------------------------------
            // Device进程
            // ------------------------------
            setDevPid(getpid());

            char exe_path[PATH_MAX];
            ssize_t count = readlink("/proc/self/exe", exe_path, PATH_MAX);
            if (count == -1) {
                perror("[DEVICE] readlink failed");
                _exit(EXIT_FAILURE);
            }
            exe_path[count] = '\0';
            char* dir_path = dirname(exe_path);

            std::string device_dir = std::string(dir_path) + "/../device";
            std::string target_bin = device_dir + "/test_hccp_service.bin";
            std::string common_lib_dir = std::string(dir_path) + "/../common";

            setenv("LD_LIBRARY_PATH", (common_lib_dir + ":" + device_dir).c_str(), 1);

            char pid_param[32] = {0};
            snprintf(pid_param, sizeof(pid_param), "--pid=%d", corresponding_host_pid);
            char phys_id_param[32] = {0};
            snprintf(phys_id_param, sizeof(phys_id_param), "--deviceId=%d", phy_id);

            std::cout << "[DEVICE-" << phy_id << "] Start (host_pid=" << corresponding_host_pid << ", pid=" << getpid() << ")\n";

            execl(target_bin.c_str(),
                  "test_hccp_service.bin",
                  "--hdcType=18",
                  pid_param,
                  phys_id_param,
                  "--logLevelInPid=3",
                  nullptr);

            perror("[DEVICE] execl failed");
            _exit(EXIT_FAILURE);
        } else {
            // ------------------------------
            // Host进程
            // ------------------------------
            std::cout << "[HOST-" << phy_id << "] Spawned device (pid=" << pid << ")\n";
            return pid;
        }
    }

    bool SpawnHostProcess(int phy_id, HostTestLogic host_logic) {
        host_logic_map_[phy_id] = std::move(host_logic);

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "[MAIN] Fork host failed for phy_id=" << phy_id << std::endl;
            return false;
        }

        if (pid == 0) {
            // ------------------------------
            // Host子进程逻辑
            // ------------------------------
            std::cout << "[HOST-" << phy_id << "] Start (pid=" << getpid() << ")\n";
            
            // 1. 初始化HAL
            InitHalStubForHost();

            // 2. 拉起 Device 进程 (由 Host 自己负责)
            pid_t dev_pid = SpawnDeviceProcess(phy_id);
            if (dev_pid == -1) {
                std::cerr << "[HOST-" << phy_id << "] Failed to spawn device\n";
                _exit(EXIT_FAILURE);
            }

            // 3. 执行注入的测试逻辑 (使用 try/catch 保证 Device 被清理)
            int exit_code = EXIT_SUCCESS;
            try {
                host_logic_map_[phy_id](phy_id);
                std::cout << "[HOST-" << phy_id << "] Logic finished successfully\n";
            } catch (const std::exception& e) {
                std::cerr << "[HOST-" << phy_id << "] Exception: " << e.what() << "\n";
                exit_code = EXIT_FAILURE;
            } catch (...) {
                std::cerr << "[HOST-" << phy_id << "] Unknown exception\n";
                exit_code = EXIT_FAILURE;
            }

            // 4. 清理 Device 进程
            CleanupDeviceProcess(phy_id, dev_pid);

            // 5. 退出
            _exit(exit_code);
        } else {
            // ------------------------------
            // 主进程逻辑
            // ------------------------------
            host_pids_[phy_id] = pid;
            std::cout << "[MAIN] Spawned host (phy_id=" << phy_id << ", pid=" << pid << ")\n";
            return true;
        }
    }

protected:
    void SetUp() override {
        main_host_pid_ = getpid();
        phys_ids_.clear();
    }

    void TearDown() override {
        // 主进程现在只需要清理 Host 进程，不需要管 Device 了
        for (auto& pair : host_pids_) {
            int phy_id = pair.first;
            pid_t pid = pair.second;
            if (pid == -1) continue;

            int status = 0;
            std::cout << "[CLEANUP] Killing host (phy_id=" << phy_id << ", pid=" << pid << ")\n";
            
            kill(pid, SIGUSR1);
            pid_t result = waitpid(pid, &status, 0);
            
            if (result == -1) {
                std::cerr << "[CLEANUP] waitpid host failed\n";
            } else {
                std::cout << "[CLEANUP] Host (phy_id=" << phy_id << ") exited: ";
                PrintExitStatus(status);
                
                if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                    FAIL() << "Host (phy_id=" << phy_id << ") exited with non-success status";
                }
            }
        }
        host_pids_.clear();
        host_logic_map_.clear();
    }
};

TEST_F(HccpSystemTest, MultiHostTlsTest)
{
    const std::vector<int> test_phyids = {0};

    for (int phy_id : test_phyids) {
        bool ok = SpawnHostProcess(phy_id, [](int phyid) {
            std::cout << "[HOST-" << phyid << "] Running RaInit...\n";

            RaInitConfig config;
            config.phyId = phyid;
            config.nicPosition = 1;
            config.hdcType = 18;
            config.enableHdcAsync = false;

            int ret = RaInit(&config);
            if (ret != 0) {
                std::cerr << "[HOST-" << phyid << "] RaInit failed ret=" << ret << "\n";
                _exit(EXIT_FAILURE);
            }

            bool tls_enable = false;
            RaInfo info{.mode = 1, .phyId = phyid};

            std::cout << "[HOST-" << phyid << "] Running RaGetTlsEnable...\n";
            ret = RaGetTlsEnable(&info, &tls_enable);
            if (ret != 0) {
                std::cerr << "[HOST-" << phyid << "] RaGetTlsEnable failed ret=" << ret << "\n";
                RaDeinit(&config);
                _exit(EXIT_FAILURE);
            }

            ret = RaDeinit(&config);
            if (ret != 0) {
                std::cerr << "[HOST-" << phyid << "] RaDeinit failed ret=" << ret << "\n";
                _exit(EXIT_FAILURE);
            }

            std::cout << "[HOST-" << phyid << "] TLS enable result: " << tls_enable << "\n";
        });
        ASSERT_TRUE(ok) << "Spawn host failed for phy_id=" << phy_id;
    }

    sleep(5);
}
