# CommEngine

## 功能说明

通信引擎类型枚举。

## 定义原型

```c
typedef enum {
    COMM_ENGINE_RESERVED = -1,    /* 保留的通信引擎，暂不支持配置 */
    COMM_ENGINE_CPU = 0,      /* HOST CPU引擎，暂不支持配置 */
    COMM_ENGINE_CPU_TS = 1,   /* HOST CPU TS引擎，仅Atlas A2 训练系列产品/Atlas A2 推理系列产品支持 */
    COMM_ENGINE_AICPU = 2,        /* AICPU引擎，暂不支持配置 */
    COMM_ENGINE_AICPU_TS = 3,     /* AICPU TS引擎，仅Atlas A3 训练系列产品/Atlas A3 推理系列产品、Ascend 950PR/Ascend 950DT支持 */
    COMM_ENGINE_AIV = 4,          /* AIV引擎，仅Ascend 950PR/Ascend 950DT支持 */
    COMM_ENGINE_CCU = 5,          /* CCU引擎，仅Ascend 950PR/Ascend 950DT支持 */
} CommEngine;
```
