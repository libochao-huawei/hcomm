# ChannelEntity

## 功能说明

Device侧通道实体结构体，用于描述一个通信通道在device侧所需的全部资源信息。

## 定义原型

```c
typedef struct {
    CommAbiHeader abiHeader;
    CommEngine engine;
    CommProtocol protocol;
    uint32_t localNotifyNum;
    uint32_t remoteNotifyNum;
    uint32_t localBufferNum;
    uint32_t remoteBufferNum;
    uint32_t sqNum;
    uint32_t cqNum;
    RegedNotifyEntity *localNotifyAddr;
    RegedNotifyEntity *remoteNotifyAddr;
    RegedBufferEntity *localBufferAddr;
    RegedBufferEntity *remoteBufferAddr;
    SqContext *sqContextAddr;
    CqContext *cqContextAddr;
    uint8_t reserve[160];
} ChannelEntity;
```

## 成员说明

| 成员 | 说明 |
| --- | --- |
| abiHeader | ABI头部信息，包含版本号和魔数等。<br>CommAbiHeader类型的定义请参见[CommAbiHeader](CommAbiHeader.md)。 |
| engine | 通信引擎类型。<br>CommEngine类型的定义请参见[CommEngine](CommEngine.md)。 |
| protocol | 通信协议类型。<br>CommProtocol类型的定义请参见[CommProtocol](CommProtocol.md)。 |
| localNotifyNum | 本端通知消息数组的元素个数。 |
| remoteNotifyNum | 远端通知消息数组的元素个数。 |
| localBufferNum | 本端注册内存数组的元素个数。 |
| remoteBufferNum | 远端注册内存数组的元素个数。 |
| sqNum | 发送队列（SQ）上下文数组的元素个数。 |
| cqNum | 完成队列（CQ）上下文数组的元素个数。 |
| localNotifyAddr | 指向device侧本端通知消息数组的指针。<br>数组元素类型为[RegedNotifyEntity](#RegedNotifyEntity)。 |
| remoteNotifyAddr | 指向device侧远端通知消息数组的指针。<br>数组元素类型为[RegedNotifyEntity](#RegedNotifyEntity)。 |
| localBufferAddr | 指向device侧本端注册内存数组的指针。<br>数组元素类型为[RegedBufferEntity](#RegedBufferEntity)。 |
| remoteBufferAddr | 指向device侧远端注册内存数组的指针。<br>数组元素类型为[RegedBufferEntity](#RegedBufferEntity)。 |
| sqContextAddr | 指向device侧发送队列上下文数组的指针。<br>数组元素类型为[SqContext](#SqContext)。 |
| cqContextAddr | 指向device侧完成队列上下文数组的指针。<br>数组元素类型为[CqContext](#CqContext)。 |
| reserve | 保留字段，用于未来扩展。 |

---

# RegedNotifyEntity

## 功能说明

注册通知消息实体，描述一个通知消息资源的属性信息。

## 定义原型

```c
typedef enum {
    REGED_NOTIFY_INVALID = -1,
    REGED_NOTIFY_IPC_RT = 0,
    REGED_NOTIFY_IPC_MEM = 1,
    REGED_NOTIFY_RMA_RT = 2,
    REGED_NOTIFY_RMA_MEM = 3,
} RegedNotifyType;

typedef struct {
    ProtectionType type;
    union {
        struct {
            uint32_t lkey;
            uint32_t rkey;
        } roce;
        struct {
            uint32_t tokenId;
            uint32_t tokenValue;
        } ub;
        uint8_t raws[24];
    } memInfo;
} ProtectionInfo;

typedef struct {
    RegedNotifyType type;
    union {
        struct {
            uint64_t addr;
            uint32_t size;
            int32_t notifyId;
        } ipcRt;
        struct {
            uint64_t addr;
            uint32_t size;
        } ipcMem;
        struct {
            uint64_t addr;
            uint32_t size;
            int32_t notifyId;
            ProtectionInfo protectionInfo;
        } rmaRt;
        struct {
            uint64_t addr;
            uint32_t size;
            ProtectionInfo protectionInfo;
        } rmaMem;
        uint8_t raws[56];
    } notifyInfo;
} RegedNotifyEntity;
```

## 成员说明

| 成员 | 说明 |
| --- | --- |
| type | 通知消息类型，决定notifyInfo联合体中哪个成员有效。 |
| notifyInfo.ipcRt | IPC RT类型通知：包含addr（地址）、size（大小）、notifyId（通知ID）。 |
| notifyInfo.ipcMem | IPC MEM类型通知：包含addr（地址）、size（大小）。 |
| notifyInfo.rmaRt | RMA RT类型通知：包含addr（地址）、size（大小）、notifyId（通知ID）、protectionInfo（保护信息）。 |
| notifyInfo.rmaMem | RMA MEM类型通知：包含addr（地址）、size（大小）、protectionInfo（保护信息）。 |

---

# RegedBufferEntity

## 功能说明

注册内存实体，描述一块注册内存的属性信息。

## 定义原型

```c
typedef enum {
    REGED_BUFFER_INVALID = -1,
    REGED_BUFFER_IPC = 0,
    REGED_BUFFER_RMA = 1,
} RegedBufferType;

typedef struct {
    RegedBufferType type;
    union {
        struct {
            uint64_t addr;
            uint64_t size;
        } ipc;
        struct {
            uint64_t addr;
            uint64_t size;
            ProtectionInfo protectionInfo;
        } rma;
        uint8_t raws[56];
    } bufferInfo;
} RegedBufferEntity;
```

## 成员说明

| 成员 | 说明 |
| --- | --- |
| type | 内存类型，决定bufferInfo联合体中哪个成员有效。 |
| bufferInfo.ipc | IPC类型内存：包含addr（地址）、size（大小）。 |
| bufferInfo.rma | RMA类型内存：包含addr（地址）、size（大小）、protectionInfo（保护信息）。 |

---

# SqContext

## 功能说明

发送队列上下文，描述一个发送队列（Send Queue）的硬件资源信息。

## 定义原型

```c
typedef enum {
    SQ_CONTEXT_TYPE_INVALID = -1,
    SQ_CONTEXT_TYPE_UB_JFS = 0,
    SQ_CONTEXT_TYPE_ROCE = 1,
} SqContextType;

typedef struct {
    SqContextType type;
    union {
        struct {
            uint64_t sqVa;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
            uint32_t jfsID;
            uint32_t wqeSize;
            uint32_t sqDepth;
            uint32_t tpID;
            uint8_t remoteEID[16];
        } ubJfs;
        struct {
            uint64_t sqVa;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
            uint32_t qpn;
            uint32_t wqeSize;
            uint32_t depth;
            int8_t dbMode;
            uint8_t sl;
        } roceSq;
        uint8_t raws[120];
    } contextInfo;
} SqContext;
```

## 成员说明

| 成员 | 说明 |
| --- | --- |
| type | SQ上下文类型，决定contextInfo联合体中哪个成员有效。 |
| contextInfo.ubJfs | UB JFS类型SQ：包含sqVa（SQ基地址）、headAddr（head指针地址）、tailAddr（tail指针地址）、dbVa（doorbell地址）、jfsID、wqeSize、sqDepth、tpID、remoteEID。 |
| contextInfo.roceSq | RoCE类型SQ：包含sqVa（SQ基地址）、headAddr（head指针地址）、tailAddr（tail指针地址）、dbVa（doorbell地址）、qpn（QP号）、wqeSize、depth、dbMode、sl（服务等级）。 |

---

# CqContext

## 功能说明

完成队列上下文，描述一个完成队列（Completion Queue）的硬件资源信息。

## 定义原型

```c
typedef enum {
    CQ_CONTEXT_TYPE_INVALID = -1,
    CQ_CONTEXT_TYPE_UB_JFC = 0,
    CQ_CONTEXT_TYPE_ROCE = 1,
} CqContextType;

typedef struct {
    CqContextType type;
    union {
        struct {
            uint64_t scqVa;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
            uint32_t jfcID;
            uint32_t cqeSize;
            uint32_t cqDepth;
        } ubJfc;
        struct {
            uint64_t cqVa;
            uint64_t headAddr;
            uint64_t tailAddr;
            uint64_t dbVa;
            uint32_t cqn;
            uint32_t cqeSize;
            uint32_t cqDepth;
            int8_t dbMode;
        } roceCq;
        uint8_t raws[120];
    } contextInfo;
} CqContext;
```

## 成员说明

| 成员 | 说明 |
| --- | --- |
| type | CQ上下文类型，决定contextInfo联合体中哪个成员有效。 |
| contextInfo.ubJfc | UB JFC类型CQ：包含scqVa（SCQ基地址）、headAddr（head指针地址）、tailAddr（tail指针地址）、dbVa（doorbell地址）、jfcID、cqeSize、cqDepth。 |
| contextInfo.roceCq | RoCE类型CQ：包含cqVa（CQ基地址）、headAddr（head指针地址）、tailAddr（tail指针地址）、dbVa（doorbell地址）、cqn（CQ号）、cqeSize、cqDepth、dbMode。 |
