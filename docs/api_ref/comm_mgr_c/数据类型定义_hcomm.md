# 数据类型定义

## HcclResult

定义集合通信相关操作的返回值。

```c
typedef enum {
    HCCL_SUCCESS = 0,               /* success */
    HCCL_E_PARA = 1,                /* parameter error */
    HCCL_E_PTR = 2,                 /* empty pointer */
    HCCL_E_MEMORY = 3,              /* memory error */
    HCCL_E_INTERNAL = 4,            /* internal error */
    HCCL_E_NOT_SUPPORT = 5,         /* not support feature */
    HCCL_E_NOT_FOUND = 6,           /* not found specific resource */
    HCCL_E_UNAVAIL = 7,             /* resource unavailable */
    HCCL_E_SYSCALL = 8,             /* call system interface error */
    HCCL_E_TIMEOUT = 9,             /* timeout */
    HCCL_E_OPEN_FILE_FAILURE = 10,  /* open file fail */
    HCCL_E_TCP_CONNECT = 11,        /* tcp connect fail */
    HCCL_E_ROCE_CONNECT = 12,       /* roce connect fail */
    HCCL_E_TCP_TRANSFER = 13,       /* tcp transfer fail */
    HCCL_E_ROCE_TRANSFER = 14,      /* roce transfer fail */
    HCCL_E_RUNTIME = 15,            /* call runtime api fail */
    HCCL_E_DRV = 16,                /* call driver api fail */
    HCCL_E_PROFILING = 17,          /* call profiling api fail */
    HCCL_E_CCE = 18,                /* call cce api fail */
    HCCL_E_NETWORK = 19,            /* call network api fail */
    HCCL_E_AGAIN = 20,              /* try again */
    HCCL_E_REMOTE = 21,             /* error cqe */
    HCCL_E_SUSPENDING = 22,         /* error communicator suspending */
    HCCL_E_OPRETRY_FAIL = 23,       /* retry constraint */
    HCCL_E_OOM = 24,                /* out of memory */
    HCCL_E_RESERVED                 /* reserved */
} HcclResult;
```

## HcclReduceOp

定义集合通信reduce操作的类型。

```c
typedef enum {
    HCCL_REDUCE_SUM = 0,    /* sum */
    HCCL_REDUCE_PROD = 1,   /* prod */
    HCCL_REDUCE_MAX = 2,    /* max */
    HCCL_REDUCE_MIN = 3,    /* min */
    HCCL_REDUCE_RESERVED    /* reserved */
} HcclReduceOp;
```

## HcclComm

指向当前通信域的句柄。

```c
typedef void *HcclComm;
```

## HcclDataType

定义集合通信相关操作的数据类型。

```c
typedef enum {
    HCCL_DATA_TYPE_INT8 = 0,     /* int8 */
    HCCL_DATA_TYPE_INT16 = 1,    /* int16 */
    HCCL_DATA_TYPE_INT32 = 2,    /* int32 */
    HCCL_DATA_TYPE_FP16 = 3,     /* float16 */
    HCCL_DATA_TYPE_FP32 = 4,     /* float32 */
    HCCL_DATA_TYPE_INT64 = 5,    /* int64 */
    HCCL_DATA_TYPE_UINT64 = 6,   /* uint64 */
    HCCL_DATA_TYPE_UINT8 = 7,    /* uint8 */
    HCCL_DATA_TYPE_UINT16 = 8,   /* uint16 */
    HCCL_DATA_TYPE_UINT32 = 9,   /* uint32 */
    HCCL_DATA_TYPE_FP64 = 10,    /* fp64 */
    HCCL_DATA_TYPE_BFP16 = 11,   /* bfp16 */
    HCCL_DATA_TYPE_INT128 = 12,  /* int128 */
    HCCL_DATA_TYPE_HIF8 = 14,     /* hif8 */
    HCCL_DATA_TYPE_FP8E4M3 = 15,  /* fp8e4m3 */
    HCCL_DATA_TYPE_FP8E5M2 = 16,  /* fp8e5m2 */
    HCCL_DATA_TYPE_FP8E8M0 = 17,  /* fp8e8m0 */
    HCCL_DATA_TYPE_MXFP8 = 18,    /* mxfp8 */
    HCCL_DATA_TYPE_RESERVED      /* reserved */
} HcclDataType;
```

## HcclSendRecvType

用于批量点对点通信操作，用来标识当前的任务类型是发送还是接收。

```c
typedef enum {
    HCCL_SEND = 0,    /* 当前任务是发送任务 */
    HCCL_RECV = 1,    /* 当前任务是接收任务 */
    HCCL_SEND_RECV_RESERVED     /* 保留字段 */
} HcclSendRecvType;
```

## HcclSendRecvItem

用于批量点对点通信操作，定义每一个通信任务的基本信息，包含任务类型、数据地址、数据个数、数据类型、对端rank编号等。

```c
typedef struct HcclSendRecvItemDef {
    HcclSendRecvType sendRecvType;    /* 标识当前任务类型是发送还是接收 */
    void *buf;        /* 数据发送/接收的buffer地址 */
    uint64_t count;   /* 发送/接收数据的个数 */
    HcclDataType dataType;   /* 发送/接收数据的数据类型 */
    uint32_t remoteRank;     /* 通信域内数据接收/发送端的rank编号 */
} HcclSendRecvItem;
```




-   **[HcclResult](HcclResult.md)**  

-   **[HcclConfig](HcclConfig.md)**  

-   **[HcclConfigValue](HcclConfigValue.md)**  

-   **[HcclConfigType](HcclConfigType.md)**  

-   **[HcclOpExpansionMode](HcclOpExpansionMode.md)**  

-   **[HcclRootInfo](HcclRootInfo.md)**  

-   **[HcclComm](HcclComm.md)**  

-   **[HcclCommConfig](HcclCommConfig.md)**  

-   **[HcclCommConfigCapability](HcclCommConfigCapability.md)**  

-   **[HcclCommSymWindow](HcclCommSymWindow.md)**  

