/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_RANK_GRAPH_H
#define HCCL_RANK_GRAPH_H

#include <arpa/inet.h>
#include "hccl_res.h"
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief RankGraph类别
 * @warning  考虑A2等变更为version等
 */
typedef enum {
    RANK_GRAPH_TYPE_RESERVED = -1, ///< 保留的rank图类型
    RANK_GRAPH_TYPE_910A2 = 0,     ///< 910A2的rank图类型
    RANK_GRAPH_TYPE_910A3 = 1,     ///< 910A3的rank图类型
} RankGraphType;

/**
 * @brief 通信拓扑枚举
 * @warning 检查910A3的拓扑类型
 */
typedef enum {
    COMM_TOPO_RESERVED = -1,  ///< 保留拓扑
    COMM_TOPO_1DMESH = 1,     ///< 1DMesh互联拓扑
    COMM_TOPO_2DMESH = 2,     ///< 2DMesh互联拓扑
    COMM_TOPO_CLOS = 3,       ///< CLOS互联拓扑
    COMM_TOPO_910_93 = 4,     ///< 910_93互联拓扑
    COMM_TOPO_310P = 5,       ///< 310P
} CommTopo;

/**
 * @brief 通信的rank图类型
 */
typedef enum {
    RANK_GRAPH_RESERVED = -1,   ///< 保留的rank图类型
    RANK_GRAPH_910_93 = 0,      ///< 910_93 rank图类型
} GraphType;

/**
 * @brief 通信设备地址类别
 */
typedef enum {
    COMM_ADDR_TYPE_RESERVED = -1, ///< 保留地址类型
    COMM_ADDR_TYPE_IP_V4 = 0,     ///< IPv4地址类型
    COMM_ADDR_TYPE_IP_V6 = 1,     ///< IPv6地址类型
    COMM_ADDR_TYPE_ID = 2,        ///< ID地址类型
} CommAddrType;

/**
 * @brief 通信设备地址描述结构体
 */
typedef struct {
    CommAddrType type;  ///< 通信地址类别
    union {
        uint32_t id;            ///< 标识
        struct in_addr addr;   ///< IPv4地址结构
        struct in6_addr addr6; ///< IPv6地址结构
    };
} CommAddr;

/**
 * @brief 通信连接通道的端侧
 *
 */
typedef struct {
    uint32_t rankId;    ///< rank标识
    CommProtocol protocol;  ///< 通信协议
    CommAddr commAddr;      ///< 通信地址
} EndPoint;

/**
 * @brief Hccl的Link信息
 * @warning
 */

// 依赖HCCL_INVALID_PORT等常量，待解决
typedef struct {
    EndPoint srcEndPoint;
    EndPoint dstEndPoint;
    CommProtocol protocol;
    char *bandWidth;
} CommLink;

/**
 * @brief 获取通信域中自己的rankId（非world rank）（已对外开放，运行时和编程共用API）
 * @param[in] comm 通信域句柄
 * @param[out] rank 自己的rankId
 * @return HcclResult 执行结果状态码
 * @note 给定通信域，查询自己的rank号（本通信域内）
 * @warning extern HcclResult CommGetMyRank(HcclComm comm, uint32_t *rankId)
 */
extern HcclResult HcclGetRankId(HcclComm comm, uint32_t *rank);

/**
 * @brief 获取通信域中的rank图
 * @param comm 通信域句柄
 * @param rankGraph 通信域中的rank图
 * @return HcclResult 执行结果状态码
 * @note 外部使用rankGraph，但不能释放rankGraph内存
 * @warning
 */
extern HcclResult HcclGetRankGraph(HcclComm comm, GraphType type, void **graph, uint32_t *len);

/**
 * @brief 给定通信域，返回该通信域的rank数量
 * @param[in] comm 通信域
 * @param[out] rankSize 该通信域包含的rank数量
 * @return HcclResult 执行结果状态码
 * @note 返回该通信域的rankSize。
 * @code {.c}
 * // 例如4个910B server(8卡)的通信域
 * uint32_t rankSize;
 * HcclGetRankSize (comm, &rankSize);
 * // rankSize=32
 * @endcode
 * @warning  HcclGetRankList是否要和HcclGetRankList合并成一个接口？ 和CommGetLevels统一。
 */
extern HcclResult HcclGetRankSize(HcclComm comm, uint32_t *rankSize);


/**
 * @brief 给定通信域，查询本rank在该通信域中的网络层次，返回分层信息
 * @param[in] comm 通信域
 * @param[out] netLayers 通信域中包含的通信网络层次，返回一个list，包含layer编号
 * @param[out] netLayerNum 网络层次列表数量
 * @return HcclResult 执行结果状态码
 * @note 使用参考：
 * @code {.c}
 * uint32_t *netLayers;
 * uint32_t layerNum;
 * HcclGetNetLayers(comm, &netLayers, &layerNum);
 * // 以910A2 server内，server间两级拓扑为例
 * // netLayers = [0,1], layerNum = 2
 * @endcode
 */
extern HcclResult HcclGetNetLayers(HcclComm comm, uint32_t **netLayers, uint32_t *netLayerNum);

/**
 * @brief 给定通信域和netLayer，返回rank数量
 * @param[in] comm 通信域句柄
 * @param[in] netLayer 通信网络层次
 * @param[out] rankNum 该netLayer的rank数量
 * @return HcclResult 执行结果状态码
 * @note 以910A2，4server为例，共32个rank，分为两级，8（server内）*4（server数量）\n使用参考：
 * @code {.c}
 * hcclComm commTp = createComm([0,1,2,3,…,31]);
 * Uint32 rankNum;
 * CommGetLevelRankSize( commTp, level=0, &rankNum )
 * // rankNum=8
 * CommGetLevelRankSize( commTp, level=1, &rankNum )
 * // rankNum=32
 * @endcode
 * 主要用于不需要返回list的场景，只返回size即可；对于超大规模的集群，
 * 返回list会消耗较多的时间和内存
 */
extern HcclResult HcclGetInstSizeByNetLayer(HcclComm comm,
    uint32_t netLayer, uint32_t *rankNum);

/**
 * @brief 给定通信域和netLayer，查询rankTable在该层分为多少group，以及每个group的size
 * @param[in] comm 通信域
 * @param[in] netLayer 通信网络层次
 * @param[out] instSizeList 所有inst的size组成一个列表
 * @param[out] listSize 列表大小
 * @return HcclResult 执行结果状态码
 * @note 以8（server内）*4（server间）的拓扑为例。使用参考：
 * @code {.cc}
 * commA = createComm([0,1,2,…,31]);
 * uint32_t *sizeList;
 * uint32_t num;
 * HcclGetInstSizeListByNetLayer( commA, netLayer=0, &sizeList, &num );
 * // sizeList=[8,8,8,8], num=4
 * HcclGetInstSizeListByNetLayer ( commA, netLayer=1, &sizeList, &num );
 * // sizeList = [32], num=1
 * @endcode
 */
extern HcclResult HcclGetInstSizeListByNetLayer(HcclComm comm, uint32_t netLayer, uint32_t **instSizeList,
    uint32_t *listSize);

/**
 * @brief 给定通信域和netLayer，查询本rank在该netLayer的硬件连接拓扑
 * @param[in] comm 通信域
 * @param[in] netLayer 通信网络层次
 * @param[out] topoType topo类型，包括1DMesh/2DMesh/clos等
 * @return HcclResult 执行结果状态码
 * @warning  1、需要将topoType转换成枚举，字符串等？。
 * @note 以910B，4server为例，共32个rank，分为两级，8（server内，为1DMesh）*4（server数量）
 * 32个Rank在netLayer1为RDMA全互联（clos网络）
 * @code {.c}
 * commTp = createComm([0,1,2,..,31]);
 * uint32_t topoType;
 * HcclGetInstTopoTypeByNetLayer( commTp, netLayer=0, &topoType ); // topoType=0 (1DMesh)
 * HcclGetInstTopoTypeByNetLayer( commTp, netLayer=1, &topoType ); // topoType=2 (clos)
 * @endcode
 */
extern HcclResult HcclGetInstTopoTypeByNetLayer(HcclComm comm, uint32_t netLayer, CommTopo *topoType);

/**
 * @brief 查询制定层次，源和目的之间的link信息
 * @param[in] comm 通信域
 * @param[in] netLayer 通信网络层次
 * @param[in] srcRank 源rank ID
 * @param[in] dstRank 目的rank ID
 * @param[out] linkList 通信链路列表
 * @param[out] listSize 列表大小
 * @return HcclResult 执行结果状态码
 * @warning
 */
extern HcclResult HcclGetLinks(HcclComm comm, uint32_t netLayer, uint32_t srcRank, uint32_t dstRank,
    CommLink **linkList, uint32_t *listSize);

/**
 * @brief 给定通信域和netLayer，返回本Rank所在的netInstance中的所有ranks
 * @param[in] comm 通信域
 * @param[in] netLayer 通信网络层次
 * @param[out] rankList 该层netLayer中包含的rankId列表
 * @param[out] rankNum rankId列表数量
 * @return HcclResult 执行结果状态码
 * @note 使用参考：
 * @code {.c}
 * 以910B，4server为例，共32个rank，分为两级，8（server内）*4（server数量）
 * Rank0
 * hcclComm commTp = createComm([0,1,2,3,…,31]);
 * Vector<uint32_t> rankList;
 * uint32_t rankNum;
 * 如果本卡为rank0
 * HcclGetInstRanksByNetLayer( commTp, netLayer=0, &rankList, &rankNum ) 
 * // rankList = [0,1,2,…,7],  rankNum=8
 * HcclGetInstRanksByNetLayer( commTp, netLayer=1, &rankList, &rankNum ) 
 * // rankList = [0,1,2,…,31],  rankNum=32
 * 
 * 如果本卡为rank9
 * HcclGetInstRanksByNetLayer( commTp, netLayer=0, &rankList, &rankNum ) 
 * // rankList = [8,9,10,…,15],  rankNum=8
 * HcclGetInstRanksByNetLayer( commTp, netLayer=1, &rankList, &rankNum ) 
 * // rankList = [0,1,2,…,31],  rankNum=32
 * @endcode
 * 说明：该接口只反映组网/拓扑情况，不反映算法情况，所以这里的netLayer1查询的是level1可连通的范围，
 * 查询结果List里是32张卡，而不是4张卡。\n
 * 例如算法选择单级全连接算法，就只使用netLayer1的链路
 */
extern HcclResult HcclGetInstRanksByNetLayer(HcclComm comm,
    uint32_t netLayer, uint32_t **rankList, uint32_t *rankNum);
#ifdef __cplusplus
}
#endif  // __cplusplus
#endif
