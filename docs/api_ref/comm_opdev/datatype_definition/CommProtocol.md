# CommProtocol

## 功能说明

定义通信协议类型枚举。

## 定义原型

```c
typedef enum {
    COMM_PROTOCOL_RESERVED = -1,  /* 保留协议类型 */
    COMM_PROTOCOL_HCCS = 0,       /* HCCS协议 */
    COMM_PROTOCOL_ROCE = 1,       /* RDMA over Converged Ethernet */
    COMM_PROTOCOL_PCIE = 2,       /* PCIe协议 */
    COMM_PROTOCOL_SIO = 3,        /* SIO协议 */
    COMM_PROTOCOL_UBC_CTP = 4,    /* 华为统一总线UBC_CTP */
    COMM_PROTOCOL_UBC_TP = 5,     /* 华为统一总线UBC_TP */
    COMM_PROTOCOL_UB_MEM = 6,     /* UB_MEM */
} CommProtocol;
```
