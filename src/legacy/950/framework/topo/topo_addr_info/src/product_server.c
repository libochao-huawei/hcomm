/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "product_server.h"
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include "rank_info_types.h"
#include "hal.h"
#include "topo.h"
#include "host_rdma.h"
#include "eid_util.h"
#include "securec.h"

#define MAX_SERVER_ROOTINFO_LEN (2048)
#define PRODUCT_MESH_LEVEL (0)
#define PRODUCT_CLOS_LEVEL (1)
#define PRODUCT_ROCE_LEVEL (3)
#define MAX_MESH_PORT_ID (9)

/* 用于识别FE*/
#define MAX_UE_IN_LEVEL (2)
#define MAX_LEVEL_NUM (4)
#define UE_TYPE_MESH (0)
#define UE_TYPE_CLOS (1)

typedef struct stUEInfo {
    int dieId;
    int feId;
    int type;
    int portNum;
    int ports[MAX_PORT_NUM];
}UEInfo;

typedef int (*GetNetInstanceIdFunc)(int npu_id, const struct dcmi_spod_info *spodInfo, char *netInstanceId, int netInstanceIdLen);

typedef struct stLevelInfo {
    int level;
    int ueNum;
    UEInfo ueList[MAX_UE_IN_LEVEL];
    char netType[16];
    GetNetInstanceIdFunc instanceIdFunc;
}LevelInfo;

typedef struct _stUBRule {
    unsigned int mainBoardId;
    int levelNum;
    LevelInfo levelInfos[MAX_LEVEL_NUM];
}NetInfo;

/**
 *  获取OS级net instance id， 用于不组超节点形态
 */
int GetNetInstanceIdForOS(int npu_id, const struct dcmi_spod_info *spodInfo, char *netInstanceId, int netInstanceIdLen);

/**
 *  获取超节点中单个服务器的net instance id
 */
int GetNetInstanceIdForPod(int npu_id, const struct dcmi_spod_info *spodInfo, char *netInstanceId, int netInstanceIdLen);

/**
 *  获取超节点的net instance id
 */
int GetNetInstanceIdForSuperPod(int npu_id, const struct dcmi_spod_info *spodInfo, char *netInstanceId, int netInstanceIdLen);

static const NetInfo g_netInfoList[] = {
    {
        .mainBoardId = MAIN_BOARD_ID_SERVER_UBX,
        .levelNum = 1, 
        {
            {
                .level = 0,
                .netType = NET_TYPE_TOPO_FILE_DESC,
                .ueNum = 2,
                .instanceIdFunc = GetNetInstanceIdForOS,
                .ueList = {
                    {.dieId = UDIE_1, .feId = 3, .type =UE_TYPE_MESH, .portNum = 0}, 
                    {.dieId = UDIE_1, .feId = 2, .type =UE_TYPE_CLOS, .portNum = 4, .ports = {4,5,6,7}},
                }
            },
        },
   },
   {
        .mainBoardId = MAIN_BOARD_ID_SERVER_8PMESH,
        .levelNum = 2, 
        {
            {  // level 0 为OS内通信
                .level = 0,
                .netType = NET_TYPE_TOPO_FILE_DESC,
                .ueNum = 1,
                .instanceIdFunc = GetNetInstanceIdForPod,
                .ueList = {
                    {.dieId = UDIE_1, .feId = 5, .type = UE_TYPE_MESH, .portNum = 0},
                }
            },
            { // level 1 为超平面
                .level = 1,
                .netType = NET_TYPE_CLOS,
                .ueNum = 1,
                .instanceIdFunc = GetNetInstanceIdForSuperPod,
                .ueList = { // 8口scaleup
                    {.dieId = UDIE_0, .feId = 3, .type =UE_TYPE_CLOS, .portNum = 8, .ports = {1,2,3,4,5,6,7,8}},
                },
            },
        },
   },
   {
        .mainBoardId = MAIN_BOARD_ID_SERVER_8PMESH_UBOE,
        .levelNum = 2, 
        {
            {
                .level = 0, .netType = NET_TYPE_TOPO_FILE_DESC, .ueNum = 1,
                .instanceIdFunc = GetNetInstanceIdForPod,
                .ueList = { {.dieId = UDIE_1, .feId = 5, .type = UE_TYPE_MESH, .portNum = 0}, }
            },
            {
                .level = 1, .netType = NET_TYPE_CLOS, .ueNum = 1,
                .instanceIdFunc = GetNetInstanceIdForSuperPod,
                .ueList = { {.dieId = UDIE_0, .feId = 3, .type =UE_TYPE_CLOS, .portNum = 8,
                    .ports = {1,2,3,4,5,6,7,8}}, },
            },
        },
   },
   {
        .mainBoardId = MAIN_BOARD_ID_SERVER_8PMESH_NOSP_UBOE,
        .levelNum = 1, 
        {
            {
                .level = 0,
                .netType = NET_TYPE_TOPO_FILE_DESC,
                .ueNum = 1,
                .instanceIdFunc = GetNetInstanceIdForOS,
                .ueList = {
                    {.dieId = UDIE_1, .feId = 5, .type = UE_TYPE_MESH, .portNum = 0},
                }
            }
        },
   },
   {
        .mainBoardId = MAIN_BOARD_ID_SERVER_8PMESH_NOSP,
        .levelNum = 1,// 这种形态只有一层网络
        {
            {
                .level = 0, .netType = NET_TYPE_TOPO_FILE_DESC, .ueNum = 1,
                .instanceIdFunc = GetNetInstanceIdForOS,
                .ueList = { {.dieId = UDIE_1, .feId = 5, .type = UE_TYPE_MESH, .portNum = 0}, }
            }
        },
   },
   {
        .mainBoardId = MAIN_BOARD_ID_SERVER_TYPE1,
        .levelNum = 2, 
        {
            {
                .level = 0,
                .ueNum = 1,
                .ueList = {
                    {.dieId = 1, .feId = 5, .type = UE_TYPE_MESH, .portNum = 0},
                }
            },
            {
                .level = 1,
                .ueNum = 2,
                .ueList = {
                    {.dieId = 0, .feId = 3, .type =UE_TYPE_CLOS, .portNum = 8, .ports = {4,5,6,7}},
                    {.dieId = 1, .feId = 2, .type =UE_TYPE_CLOS, .portNum = 8, .ports = {5,6}},
                }
            },
        },
   },
};

int ServerGetRootinfoLen(size_t *len)
{
    *len = MAX_SERVER_ROOTINFO_LEN;
    return 0;
}

int GetNetInstanceIdForOS(int npu_id, const struct dcmi_spod_info *spodInfo, char *netInstanceId, int netInstanceIdLen)
{
    return get_server_id(netInstanceId, netInstanceIdLen);
}

int GetNetInstanceIdForPod(int npu_id, const struct dcmi_spod_info *spodInfo, char *netInstanceId, int netInstanceIdLen)
{
    return sprintf_s(netInstanceId, netInstanceIdLen, "sp_%ld_srv_%ld", spodInfo->super_pod_id, spodInfo->server_index);
}

int GetNetInstanceIdForSuperPod(int npu_id, const struct dcmi_spod_info *spodInfo, char *netInstanceId, int netInstanceIdLen)
{
    return sprintf_s(netInstanceId, netInstanceIdLen, "sp_%ld", spodInfo->super_pod_id);
}

const NetInfo *GetNetInfo(unsigned int mainBoardId)
{
    for (size_t i = 0; i < sizeof(g_netInfoList) / sizeof(g_netInfoList[0]); ++i) {
        if (g_netInfoList[i].mainBoardId == mainBoardId) {
            return &g_netInfoList[i];
        }
    }
    return NULL;
}

/*
 * @brief 处理mesh entity的地址, 讲UBEntity中的地址加到layer中
 * @param layer: 网络层
 * @param ue: ue entity
 * @param spod_info: spod信息
 * @return int: 0 成功
 */
static int LayerAddMesh(NetLayer *layer, UBEntity *ue, int dieId)
{
    for (unsigned int j = 0; j < ue->eidNum; ++j) {
        int phyPortId = UrmaEidGetPortId(&ue->eidList[j].eid);
        if (phyPortId > MAX_MESH_PORT_ID) {
            continue;
        }
        Addr addr;
        memset_s(&addr, sizeof(Addr), 0x00, sizeof(Addr));
        AddrSetEID(&addr, &ue->eidList[j].eid);
        char port[MAX_PORT_LEN] = {0};
        char planeId[MAX_PLANE_ID_LEN] = {0};
        // topo中端口从0开始编，CNA中需要规避全0，从1开始
        sprintf_s(port, MAX_PORT_LEN, "%d/%d", dieId, (phyPortId - 1));
        sprintf_s(planeId, sizeof(planeId), "plane_%d", dieId);
        AddrAddPort(&addr, port);
        AddrSetPlaneId(&addr, planeId);
        NetLayerAddAddr(layer, &addr);
    }
    return 0;
}

static int LayerAddClos(NetLayer *layer, UBEntity *ue, int dieId, const UEInfo *ueInfo)
{
    int portGroupIdx = UBEntityGetServerPortGroupIdx(ue);
    if (portGroupIdx < 0) {
        return -1;
    }
    Addr addr;
    memset_s(&addr, sizeof(Addr), 0x00, sizeof(Addr));
    AddrSetEID(&addr, &ue->eidList[portGroupIdx].eid);
    for (int j = 0; j < ueInfo->portNum; ++j) {
        char port[MAX_PORT_LEN] = {0};
        sprintf_s(port, MAX_PORT_LEN, "%d/%d", dieId, ueInfo->ports[j]);
        AddrAddPort(&addr, port);
    }
    char planeId[MAX_PLANE_ID_LEN] = {0};
    sprintf_s(planeId, sizeof(planeId), "plane_%d", dieId);
    AddrSetPlaneId(&addr, planeId);
    NetLayerAddAddr(layer, &addr);
    return 0;
}

/**
 * @brief 根据dieId和feId从UEList中获取UBEntity
 * @param ueList: ue entity列表
 * @param dieId: die id
 * @param ueId: ue id
 * @return UBEntity*: ue entity
*/
UBEntity *GetUBEntityByFilter(UEList *ueList, int dieId, int ueId)
{
    for (unsigned int i = 0; i < ueList->ueNum; i++) {
        int die = UrmaEidGetServerDieId(&ueList->ueList[i].eidList[0].eid);
        int fe = UBEntityGetId(&ueList->ueList[i]);
        if (die == dieId && fe == ueId) {
            return &ueList->ueList[i];
        }
    }
    return NULL;
}


/**
 * @brief 根据配置从ueList中过滤出ub entity, 并处理其中的地址添加到netlayer中
 * @param layer: 网络层信息
 * @param ueList: 全量的ue entity数组
 * @param levelInfo:  当前处理的层级信息
 * @param spodInfo: spod信息
 * @return int: 0 成功
 */
static int ProcessLayer(int npuId, NetLayer *layer, UEList *ueList, const LevelInfo *levelInfo, const struct dcmi_spod_info *spodInfo)
{
    char net_instance_id[MAX_INSTANCE_ID_LEN] = {0};
    levelInfo->instanceIdFunc(npuId, spodInfo, net_instance_id, sizeof(net_instance_id));
    NetLayerInit(layer, levelInfo->level, net_instance_id);
    NetLayerSetNetType(layer, levelInfo->netType);

    for (int i = 0; i < levelInfo->ueNum; ++i) {
        int fe = levelInfo->ueList[i].feId;
        int die = levelInfo->ueList[i].dieId;
        int type = levelInfo->ueList[i].type;
        UBEntity* ue = GetUBEntityByFilter(ueList, die, fe);
        if (ue == NULL) {
            continue;
        }
        if (type == UE_TYPE_MESH) {
            LayerAddMesh(layer, ue, die);
        } else if (type == UE_TYPE_CLOS) {
            LayerAddClos(layer, ue, die, &levelInfo->ueList[i]);
        }
    }
    return 0;
}

int ServerGetRootinfo(int npu_id, unsigned mainboard_id, void *buf, size_t *len)
{
    if (buf == NULL || len == NULL) {
        return RET_NOK;
    }
    RootInfo rootinfo;
    Rank rank;
    RootInfoInit(&rootinfo);
    RankInit(&rank, npu_id, npu_id);
    
    TopoGetFilePath(mainboard_id, rootinfo.topo_file_path, MAX_TOPO_PATH_LEN);
    struct dcmi_spod_info spod_info;
    UEList ueList;
    HalGetUBEntityList(npu_id, &ueList);
    hal_get_spod_info(npu_id, &spod_info);

    const NetInfo *netInfo = GetNetInfo(mainboard_id);
    if (netInfo == NULL) {
        return -1;
    }

    for (int i = 0; i < netInfo->levelNum; ++i) {
        NetLayer layer;
        if (ProcessLayer(npu_id, &layer, &ueList, &netInfo->levelInfos[i], &spod_info) == 0) {
            RankAddNetLayer(&rank, &layer);
        }
    }

    RootInfoAddRank(&rootinfo, &rank);
    char* rootinfo_buf = RootInfoToString(&rootinfo);
    if (rootinfo_buf == NULL) {
        return -1;
    }
    if ((*len) < strlen(rootinfo_buf)) {
        (*len) = strlen(rootinfo_buf);
        free(rootinfo_buf);
        return -1;
    }
    errno_t ret = strcpy_s(buf, *len, rootinfo_buf);
    (*len) = strlen(buf);
    free(rootinfo_buf);
    return ret;
}
