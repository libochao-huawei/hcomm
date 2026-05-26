# HcommChannelDesc

## 功能说明

定义组件间通道参数。

## 定义原型

```c
typedef struct {
    CommAbiHeader header;            ///< ABI头部，包含版本等信息
    EndpointDesc remoteEndpoint;     ///< 远端网络设备端侧描述
    uint32_t notifyNum;              ///< channel上使用的通知消息数量
    bool exchangeAllMems;            ///< true表示无需显式传入memHandles
    HcommMemHandle *memHandles;      ///< 注册到通信域的待交换内存句柄
    uint32_t memHandleNum;           ///< 待交换内存句柄数量
    HcommSocket socket;              ///< 预创建socket句柄
    HcommSocketRole role;            ///< 本端角色(SERVER或CLIENT)
    uint16_t port;                   ///< 监听端口或目标端口
    union {
        uint8_t raws[128];           ///< 通用缓存
        struct {
            uint32_t queueNum;       ///< QP数量
            uint32_t retryCnt;       ///< 最大重传次数
            uint32_t retryInterval;  ///< 重传间隔（ms）
            uint8_t tc;              ///< 流量类别（QoS)
            uint8_t sl;              ///< 服务等级（QoS)
            uint32_t qpThreshold;    ///< 多QP场景下，每个QP最小数据量(B)
        } roceAttr;
        struct {
            uint32_t qos;            ///< HCCS QoS
        } hccsAttr;
        struct {
            uint32_t sqDepth;         ///< UB队列深度，0表示使用默认值
        } ubAttr;
    };
} HcommChannelDesc;
```
