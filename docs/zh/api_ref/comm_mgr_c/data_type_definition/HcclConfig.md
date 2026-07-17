# HcclConfig

## 功能说明

定义集合通信相关配置。

## 定义原型

```c
typedef enum {
    HCCL_DETERMINISTIC = 0, /* 0: non-deterministic, 1: deterministic */
    HCCL_CONFIG_RESERVED
} HcclConfig;
```

## 参数说明

- HCCL_DETERMINISTIC：是否开启确定性计算。

  - 0：不开启确定性计算。
  - 1：开启确定性计算。

- HCCL_CONFIG_RESERVED：预留参数。
