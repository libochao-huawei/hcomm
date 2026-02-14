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
    int host_fd_ = -1;       // Host 端 Socket
    int device_fd_ = -1;     // 与 Device 建立连接的 Socket
    pid_t device_pid_ = -1;  // Device 进程 PID
    pid_t host_pid_ = -1;    // Host 进程 PID
    int port_ = 0;           // 动态分配的端口

    void Initialize() {
        
    }
    void InitHalStub()
    {
        setHostPid(getpid());
        setDevPid(0);
        InitHdcId();
    }

    void SetUp() override {
        InitHalStub();
        // 1. 创建 Host Socket（TCP）
        host_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(host_fd_, -1) << "Failed to create socket";

        // 2. 绑定随机端口（避免端口冲突）
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = 0;  // 系统自动分配端口
        ASSERT_NE(bind(host_fd_, (sockaddr*)&addr, sizeof(addr)), -1);

        // 3. 获取分配的端口并监听
        socklen_t len = sizeof(addr);
        getsockname(host_fd_, (sockaddr*)&addr, &len);
        port_ = ntohs(addr.sin_port);
        ASSERT_NE(listen(host_fd_, 1), -1);
        pid_t host_pid_ = getpid();

        // 4. 启动 Device 进程（传递端口号）
        device_pid_ = fork();
        ASSERT_NE(device_pid_, -1) << "Fork failed";
        if (device_pid_ == 0) {
            setDevPid(getpid());
            // 子进程
            std::string port_str = std::to_string(port_);

            // 1. 获取当前可执行文件的绝对路径
            char exe_path[PATH_MAX];
            ssize_t count = readlink("/proc/self/exe", exe_path, PATH_MAX);
            if (count == -1) {
                perror("[DEVICE] readlink failed");
                _exit(EXIT_FAILURE);
            }
            exe_path[count] = '\0';

            char* dir_path = dirname(exe_path); 
            // 2. 提取当前程序所在目录 (去掉文件名，保留目录部分)

            // 3. 拼接出 device 目录的完整路径
            std::string device_dir = std::string(dir_path) + "/../device";
            // 4. 构造目标程序的完整路径
            std::string target_bin = device_dir + "/test_hccp_service.bin";
            std::string common_lib_dir = std::string(dir_path) + "/../common";

            setenv("LD_LIBRARY_PATH", (common_lib_dir + ":" + device_dir).c_str(), 1);
            setenv("HCCP_TEST_HOST_PORT", port_str.c_str(), 1);

            std::cout << "[DEVICE] Device process started, dir: " << dir_path 
                    << ", connecting to port " << port_str << "...\n";

            // 5. 执行程序
            char pid_param[32] = {0};
            snprintf(pid_param, sizeof(pid_param), "--pid=%d", host_pid_);
            std::cout << "[DEVICE] Get host PID: " << host_pid_ << ", passing to device: " << pid_param << std::endl;
            execl(target_bin.c_str(), 
                "test_hccp_service.bin", 
                "--hdcType=18", 
                "--deviceId=0", 
                pid_param,
                "--logLevelInPid=3", 
                nullptr);

            perror("[DEVICE] execl failed");
            _exit(EXIT_FAILURE);
        } else {
            std::cout << "[HOST] device process PID: " << device_pid_ << " host PID: "<<getpid()<<std::endl;
            std::cout << "[HOST] Host process: Waiting for Device to connect on port " << port_ << "...\n";
        }
        // 5. Host 接受 Device 连接
        device_fd_ = accept(host_fd_, nullptr, nullptr);
        ASSERT_NE(device_fd_, -1) << "Accept failed";
    }

    void TearDown() override {
        if (device_fd_ != -1) close(device_fd_);
        if (host_fd_ != -1) close(host_fd_);

        // Clean up child process
        if (device_pid_ != -1) {
            int status = 0;

            // Send SIGUSR1 to child process
            if (kill(device_pid_, SIGUSR1) == -1) {
                std::cerr << "[TESTCASE] Failed to send SIGUSR1 to child process " 
                        << device_pid_ << std::endl;
            }

            // Wait for child process to exit
            pid_t result = waitpid(device_pid_, &status, 0);
            if (result == -1) {
                std::cerr << "[TESTCASE] waitpid() failed for child process " 
                        << device_pid_ << std::endl;
            } else {
                // Debug diagnostics
                std::cout << "[TESTCASE] Child process exit status:" << std::endl;
                if (WIFEXITED(status)) {
                    std::cout << "[TESTCASE] Child exited normally with code: " 
                            << WEXITSTATUS(status) << std::endl;
                } else if (WIFSIGNALED(status)) {
                    int sig = WTERMSIG(status);
                    std::cout << "[TESTCASE] Child terminated by signal: " << sig << std::endl;
                    if (sig == SIGSEGV) {
                        std::cout  <<"[TESTCASE]"<< sig << " = SIGSEGV (Segmentation fault)" << std::endl;
                    } else if (sig == SIGABRT) {
                        std::cout <<"[TESTCASE]"<< sig << " = SIGABRT (Aborted)" << std::endl;
                    } else if (sig == SIGUSR1) {
                        std::cout  <<"[TESTCASE]"<< sig << " = SIGUSR1 (testcase finished)" << std::endl;
                    }
                }
            }

            device_pid_ = -1;
    }
    }
};

TEST_F(HccpSystemTest, BasicCommunication) {
    struct RaInitConfig config {};
    int ret;
    config.phyId           = 0;
    config.nicPosition     = 1;
    config.hdcType         = 18;
    config.enableHdcAsync = false;
    std::cout << "[HOST] Calling RaInit...\n";
    ret = RaInit(&config);
    ASSERT_EQ(ret, 0) << "RaInit failed";
    bool tlsEnable = false;
    RaInfo info = {
        .mode = 1,
        .phyId = 0,
    };
    std::cout << "[HOST] Calling RaGetTlsEnable...\n";
    ret = RaGetTlsEnable(&info, &tlsEnable);
    ASSERT_EQ(ret, 0) << "RaGetTlsEnable failed";
    ret = RaDeinit(&config);
    ASSERT_EQ(ret, 0) << "RaDeinit failed";
    std::cout << "[HOST] TLS enabled: " << tlsEnable << "\n";
}