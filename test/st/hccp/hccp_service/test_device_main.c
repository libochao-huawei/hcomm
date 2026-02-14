

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
// #include "ascend_hal_stub.h"

int g_injected_socket_fd = -1;

__attribute__((constructor(200)))
void inject_socket_connection_before_main(void) {
    printf("[DEVICE] device PID initialized %d...\n", getpid());
    // setDevPid(getpid()); // 设置设备PID到共享内存，供Host读取
    // InitHdcId();
    // 1. 从环境变量获取Host的端口号（Host拉起Device时会设置这个环境变量）
    const char* port_str = getenv("HCCP_TEST_HOST_PORT");
    if (port_str == NULL) {
        fprintf(stderr, "[DEVICE] Error: HCCP_TEST_HOST_PORT environment variable not set\n");
        return;
    }
    int port = atoi(port_str);
    printf("[DEVICE] Socket injection: Connecting to Host at 127.0.0.1:%d...\n", port);

    // 2. 创建Socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("[DEVICE] Error: Failed to create socket");
        return;
    }

    // 3. 连接Host
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("[DEVICE] Error: Failed to connect to Host");
        close(sockfd);
        return;
    }

    // 4. 连接成功！保存fd到全局变量
    g_injected_socket_fd = sockfd;
    printf("[DEVICE] Socket injection: Successfully connected, fd = %d\n", g_injected_socket_fd);

    // 5. 【可选】设置Socket为非阻塞（根据业务需求）
    // int flags = fcntl(sockfd, F_GETFL, 0);
    // fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

// -------------------------- 【可选】析构函数：在main函数之后执行 --------------------------
__attribute__((destructor))
void cleanup_socket_after_main(void) {
    if (g_injected_socket_fd != -1) {
        printf("[DEVICE] Socket injection: Closing socket fd = %d...\n", g_injected_socket_fd);
        close(g_injected_socket_fd);
        g_injected_socket_fd = -1;
    }
}

// // -------------------------- 【可选】桩子函数：覆盖主工程的Socket连接函数 --------------------------
// // 如果主工程有自己的Socket连接函数（比如 hccp_create_socket()），可以用这个桩子覆盖它
// // 直接返回已经注入好的Socket fd，让业务代码无缝使用
// int hccp_create_socket(void) {
//     printf("[DEVICE] hccp_create_socket() called, returning injected fd = %d\n", g_injected_socket_fd);
//     return g_injected_socket_fd;
// }