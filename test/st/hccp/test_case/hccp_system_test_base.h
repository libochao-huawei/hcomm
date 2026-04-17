/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCP_SYSTEM_TEST_BASE_H
#define HCCP_SYSTEM_TEST_BASE_H
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <linux/limits.h>
#include <libgen.h>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <signal.h>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "hccp_sys_test_common.h"
#include "ascend_hal.h"
#include "ascend_hal_stub.h"
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#ifdef __cplusplus
}
#endif // __cplusplus

class HccpSystemTestBase : public ::testing::Test {
    protected:
        using HostTestLogic = std::function<int(int phyid)>;

        std::map<int, pid_t> host_pids_;
        std::map<int, HostTestLogic> host_logic_map_;
        std::map<int, function<int(int phyid)>> hostMock;
        std::map<int, function<int(int phyid)>> deviceMock;
        pid_t main_host_pid_ = -1;
        bool overall_test_success_ = true; 

        void MockHost(int phyId, function<int(int phyid)> func) {
            hostMock[phyId] = func;
        }

        void PrintExitStatus(int status) {
            if (WIFEXITED(status)) {
                printf("code=%d", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                printf("signal=%d", sig);
                if (sig == SIGSEGV) printf("\033[31m (SIGSEGV)\033[0m");
                else if (sig == SIGUSR1) printf("\033[1;33m (SIGUSR1)(killed by host)\033[0m");
                else if (sig == SIGABRT) {
                    printf("\033[31m (SIGABRT)\033[0m\n", sig);
                }
            }
            printf("\n");
        }

        void InitHalStubForHost(int phyId) {
            setHostPid(getpid(), phyId);
            setDevPid(0, phyId);
            InitHdcId();
        }

        void CleanupDeviceProcess(int phy_id, pid_t dev_pid) {
            if (dev_pid == -1) return;
            int status = 0;
            TEST_LOG_INFO(phy_id, "Cleaning up device (pid=%d)", dev_pid);
            kill(dev_pid, SIGUSR1);
            pid_t result = waitpid(dev_pid, &status, 0);
            if (result == -1) {
                TEST_LOG_ERROR(phy_id, "waitpid device failed");
            } else {
                printf("\033[94m[INFO]\033[0m[HOST-%d] Device exited: ", phy_id);
                PrintExitStatus(status);
            }
        }

        pid_t SpawnDeviceProcess(int phy_id) {
            pid_t corresponding_host_pid = getpid(); // 直接获取当前 Host 进程的 PID

            pid_t pid = fork();
            if (pid == -1) {
                TEST_LOG_ERROR(phy_id, "Fork device failed");
                return -1;
            }

            if (pid == 0) {
                // ------------------------------
                // Device进程
                // ------------------------------
                prctl(PR_SET_PDEATHSIG, SIGKILL);
                setDevPid(getpid(), phy_id);
    
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
                setenv("LD_PRELOAD", MOCK_LIB_PATH, 1);
                setenv("LD_LIBRARY_PATH", (common_lib_dir + ":" + device_dir).c_str(), 1);
    
                char pid_param[32] = {0};
                snprintf(pid_param, sizeof(pid_param), "--pid=%d", corresponding_host_pid);
                char phys_id_param[32] = {0};
                snprintf(phys_id_param, sizeof(phys_id_param), "--deviceId=%d", phy_id);
    
                printf("[DEVICE-%d] Start (host_pid=%d, pid=%d)\n", phy_id, corresponding_host_pid, getpid());
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
                TEST_LOG_INFO(phy_id, "Spawned device (pid=%d)", pid);
                return pid;
            }
        }

        bool SpawnHostProcess(int phy_id, HostTestLogic host_logic) {
            host_logic_map_[phy_id] = std::move(host_logic);

            pid_t pid = fork();
            if (pid == -1) {
                printf(COLOR_MAIN"[MAIN]"COLOR_RESET" Fork host failed for phy_id=%d\n", phy_id);
                return false;
            }

            if (pid == 0) {
                // ------------------------------
                // Host子进程逻辑
                // ------------------------------
                TEST_LOG_INFO(phy_id, "Start (pid=%d)", getpid());

                // 1. 初始化HAL
                InitHalStubForHost(phy_id);

                // 2. 拉起 Device 进程 
                pid_t dev_pid = SpawnDeviceProcess(phy_id);
                if (dev_pid == -1) {
                    TEST_LOG_ERROR(phy_id, "Failed to spawn device");
                    _exit(EXIT_FAILURE);
                }

                // 3. 执行注入的测试逻辑
                int exit_code = EXIT_SUCCESS;
                try {
                    exit_code = host_logic_map_[phy_id](phy_id);
                    TEST_LOG_INFO(phy_id, "test finished, exit_code=%d", exit_code);
                } catch (const std::exception& e) {
                    TEST_LOG_ERROR(phy_id, "Exception: %s", e.what());
                    exit_code = EXIT_FAILURE;
                } catch (...) {
                    TEST_LOG_ERROR(phy_id, "Unknown exception");
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
                printf("[MAIN] Spawned host (phy_id=%d, pid=%d)\n",phy_id, pid);
                return true;
            }
        }

    protected:
        void SetUp() override {
            main_host_pid_ = getpid();
            overall_test_success_ = true; 
        }

        void TearDown() override {
            const int timeout_seconds = 5;
            const useconds_t poll_interval_us = 100000; 

            for (auto& pair : host_pids_) {
                int phy_id = pair.first;
                pid_t pid = pair.second;
                if (pid == -1) continue;

                int status = 0;
                bool exited = false;
                bool timed_out = false;
                auto start_time = std::chrono::steady_clock::now();
                printf(COLOR_CLEANUP"[CLEANUP]"COLOR_RESET" Waiting for host (phy_id=%d, pid=%d)\n", phy_id, pid);

                while (true) {
                    pid_t result = waitpid(pid, &status, WNOHANG);
                    if (result > 0) { exited = true; break; } 
                    else if (result == 0) {
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
                        if (elapsed >= timeout_seconds) { timed_out = true; break; }
                        usleep(poll_interval_us);
                    } else { printf(COLOR_CLEANUP"[CLEANUP]"COLOR_RESET" waitpid(WNOHANG) error\n"); break; }
                }

                if (timed_out) {
                    printf(COLOR_CLEANUP"[CLEANUP]"COLOR_RESET" Host-%d timed out. Sending SIGUSR1...\n", phy_id);
                    kill(pid, SIGUSR1);
                    //阻塞回收进程
                    pid_t killed_result = waitpid(pid, &status, 0);
                    if (killed_result == -1) {
                        printf(COLOR_CLEANUP"[CLEANUP]"COLOR_RESET" waitpid after kill failed\n");
                    } else {
                        printf(COLOR_CLEANUP"[CLEANUP]"COLOR_RESET" Host (phy_id=%d) killed by TEST: ", phy_id);
                        PrintExitStatus(status);
                    }

                    overall_test_success_ = false;
                } else if (exited) {
                    printf(COLOR_CLEANUP"[CLEANUP]"COLOR_RESET" Host-%d finished: ", phy_id);
                    PrintExitStatus(status);
                    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                        printf(COLOR_CLEANUP"[CLEANUP]"COLOR_RESET COLOR_ERROR" Error:"COLOR_RESET" Host-%d exited with non-success status\n", phy_id);
                        overall_test_success_ = false;
                    }
                }
            }

            host_pids_.clear();
            host_logic_map_.clear();
            EXPECT_TRUE(overall_test_success_);
        }
        static void SetUpTestCase()
        {
            printf("\033[36m--HCCP SYS Test SetUP--\033[0m\n");
        }
        static void TearDownTestCase()
        {
            int shmid = shmget(SHM_KEY, 0, 0666);
            if (shmid != -1) {
                shmctl(shmid, IPC_RMID, NULL);
            }
            printf("\033[36m--HCCP SYS Test TearDown--\033[0m\n");
        }
    public:
        void RunTests(const std::vector<int>& phy_ids, HostTestLogic logic) {
            for (int phy_id : phy_ids) {
                bool ok = SpawnHostProcess(phy_id, logic);
                ASSERT_TRUE(ok) << "Spawn host failed for phy_id=" << phy_id;
            }
        }
};

#endif