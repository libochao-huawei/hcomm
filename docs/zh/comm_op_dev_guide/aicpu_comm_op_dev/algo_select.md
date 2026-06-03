# 算法选择

## 选择策略

通信算法的选择在很大程度上决定了通信算子的整体性能表现，为应对不同的网络拓扑、通信数据量以及硬件资源，一个通信算子往往具有多种算法实现。在实际分布式集群场景中，需要根据具体场景，综合考量选择拓扑亲和的通信算法，以实现通信效率的最大化。

![通信算法选择示意图](figures/aicpu_algo_select.png)

如上图所示，通信算子支持多种算法实现，开发者可以根据拓扑信息选择最优通信算法：

- Ring算法实现：算法编排在AI CPU侧，适用于Server内物理拓扑为Ring的场景。
- Mesh算法实现：算法编排在AI CPU侧，适用于Server内物理拓扑为Mesh的场景。

> [!NOTE]说明
>
>1. 如果通信算子只有一种算法实现，那么可以跳过本节的算法选择步骤。
>2. 本章所述的算法是开发者自行实现的算法，HCCL_ALGO环境变量配置的算法是HCCL内置算法。HCCL内置算法可参考《HCCL集合通信库用户指南》的相关参考 \> 集合通信算法介绍。

## 代码示例

以[选择策略](#选择策略)介绍的算法选择策略为例，对应的算法选择代码片段如下：

```c
if (algType == ALG_LEVEL0_NP_DOUBLE_RING || algType == ALG_LEVEL0_NP_SINGLE_RING) {
    algName = "Ring";  // 根据拓扑信息选择使用Ring算法
} else if (algType == ALG_LEVEL0_NP_MESH) {
    algName = "Mesh";  // 根据拓扑信息选择使用Mesh算法
} else {
    return HCCL_E_NOT_SUPPORT;
}
```
