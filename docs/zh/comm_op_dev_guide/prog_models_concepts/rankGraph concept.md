# 总体说明

![rankGraph元素](./figures/rankGraph架构示意.PNG)

## rankTable文件
  rankTable文件描述全局的网络信息
- rankTable描述全局的通信关系，分层次描述；
- rankTable中通信层次越低（越靠近netLayer0），代表通信质量越好。netLayer0通信质量大于等于netLayer1通信质量；
- rankTable采用以rank为第一视角，描述每个Rank所属的通信层次及通信网络地址EID/IP

## topo文件
- topo文件描述局部的物理信息，一般描述netLayer0对应的物理连接关系；只看到本rank所在的netLayer0的物理连接关系。
-  topo文件描述物理link，将所有物理link分类2大类，一类描述直连物理link，需要包含两端的device和port信息；另一类描述通过交换设备连接的link，只能描述一端的情况，另一端为网络，无法具体描述到设备及端口


# 操作接口
Hcomm解析ranktable文件和topo文件，形成一个内部的图数据结构rankGraph，并提供接口供HCCL开发者使用。
接口的说明可以参考[rankGraph接口](https://gitcode.com/cann/hcomm/blob/master/docs/zh/api_ref/comm_opdev/control_plane_api/topo_info_query/README.md)


# 关键概念举例
## netLayer
从端侧视角，每个rank都接入多种通信质量的网络中，如![netLayer示意](./figures/netLayer示意图.png)

## netInstance
每层NetLayer中，分为很多实例，命名为NetInstance，如![netInstance示意](./figures/netInstance示意.png)

## topoInstance
多条link形成一个有含义的硬件拓扑实例(topoInstance)，例如A5的1Dmesh直连硬件拓扑，server内的8张卡之间通过同一个topoInstance相连。

# 硬件形态举例
以A5的Server为例，图示如下![server组网示意](./figures/Server组网示意.PNG)

netLayer是从端侧视角出发的，示意如下![Server的netLayer示意](./figures/serverNetLayer示意.PNG)


