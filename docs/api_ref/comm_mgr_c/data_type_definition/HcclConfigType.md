# HcclConfigType

## 功能说明

配置通信算子的展开模式。

## 定义原型

```c
typedef enum {
    HCCL_CONFIG_TYPE_INVALID             = -1,   /* 无效配置项类型 */
    HCCL_CONFIG_TYPE_OP_EXPANSION_MODE   = 0,    /* 算子展开模式，对应类型为hcclOpExpansionMode */
} HcclConfigType;
```
