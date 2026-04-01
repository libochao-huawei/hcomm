/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "product_pod.h"
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
#define NPU_NUM (8)

int PodGetRootinfoLen(size_t *len)
{
    *len = MAX_SERVER_ROOTINFO_LEN;
    return 0;
}

typedef struct rule {
    unsigned int mainboardId;
    int level;
    int dieId;
    int ueId;
    int ports[MAX_PORT_NUM];
    int portNum;
    int npus[MAX_PORT_NUM];
    int npuNum;
    char planeId[MAX_PLANE_ID_LEN];
}UBEntityRule;

static const UBEntityRule g_ubrules[] = {
    // NPU 0-3 的1die使用6个端口
    {MAIN_BOARD_ID_POD, PRODUCT_CLOS_LEVEL, 1, 2, {0,1,2,3,5,6}, 6, {0,1,2,3}, 4, "plane_pg_0"},
    // NPU 0-3 的0die使用2个端口
    {MAIN_BOARD_ID_POD, PRODUCT_CLOS_LEVEL, 0, 2, {1,2}, 2, {0,1,2,3}, 4, "plane_pg_1"},
    // NPU 4-7 的0 die使用6个端口
    {MAIN_BOARD_ID_POD, PRODUCT_CLOS_LEVEL, 0, 2, {0,1,2,3,4,5}, 6, {4,5,6,7}, 4, "plane_pg_0"},
    // NPU 4-7 的1 die使用2个端口
    {MAIN_BOARD_ID_POD, PRODUCT_CLOS_LEVEL, 1, 2, {1,2}, 2, {4,5,6,7}, 4, "plane_pg_1"},
    // NPU 0-3 的1die使用6个端口
    {MAIN_BOARD_ID_POD_2D, PRODUCT_CLOS_LEVEL, 1, 2, {0,1,2,3,5,6}, 6, {0,1,2,3}, 4, "plane_pg_0"},
    // NPU 0-3 的0die使用2个端口
    {MAIN_BOARD_ID_POD_2D, PRODUCT_CLOS_LEVEL, 0, 2, {1,2}, 2, {0,1,2,3}, 4, "plane_pg_1"},
    // NPU 4-7 的0 die使用6个端口
    {MAIN_BOARD_ID_POD_2D, PRODUCT_CLOS_LEVEL, 0, 2, {0,1,2,3,4,5}, 6, {4,5,6,7}, 4, "plane_pg_0"},
    // NPU 4-7 的1 die使用2个端口
    {MAIN_BOARD_ID_POD_2D, PRODUCT_CLOS_LEVEL, 1, 2, {1,2}, 2, {4,5,6,7}, 4, "plane_pg_1"},
};


static int ProcessLayerMesh(int npu_id, NetLayer *layer, UEList *ueList, struct dcmi_spod_info *spod_info)
{
    NetLayerSetNetType(layer, NET_TYPE_MESH);
    char net_instance_id[MAX_INSTANCE_ID_LEN] = {0};
    sprintf_s(net_instance_id, sizeof(net_instance_id), "sp%ld_chassis%ld", spod_info->super_pod_id, spod_info->chassis_id);
    NetLayerInit(layer, PRODUCT_MESH_LEVEL, net_instance_id);


    int meshEntityId = UBGetMaxEntityId(ueList);
    const unsigned minMeshEidNum = 7;
    for (unsigned int i = 0; i < ueList->ueNum; i++) {
        int fe = UBEntityGetId(&ueList->ueList[i]);
        if (fe != meshEntityId || ueList->ueList[i].eidNum < minMeshEidNum) {
            continue;
        }
        for (unsigned int j = 0; j < ueList->ueList[i].eidNum; ++j) {
            int phyPortId = UrmaEidGetPortId(&ueList->ueList[i].eidList[j].eid);
            if (phyPortId > MAX_MESH_PORT_ID) {
                continue;
            }
            Addr addr;
            memset_s(&addr, sizeof(Addr), 0x00, sizeof(Addr));
            AddrSetEID(&addr, &ueList->ueList[i].eidList[j].eid);
            char port[MAX_PORT_LEN] = {0};
            // topo中端口从0开始编，CNA中需要规避全0，从1开始
            sprintf_s(port, MAX_PORT_LEN, "%d/%d", (npu_id % 8) < 4 ? 0 : 1, phyPortId < 2 ? (phyPortId - 1) : (phyPortId + 2));
            AddrAddPort(&addr, port);
            AddrSetPlaneId(&addr, "plane_0");
            NetLayerAddAddr(layer, &addr);
        }
    }
    return 0;
}

static const UBEntityRule* GetUBRule(int npu_id, int level, unsigned int mainBoardId, int die, int feId)
{
    for (size_t i = 0; i < sizeof(g_ubrules)/sizeof(UBEntityRule); ++i) {
        if (g_ubrules[i].level != level) {
            continue;
        }
        int isNpuInRule = 0;
        for (size_t j = 0; j < g_ubrules[i].npuNum; ++j) {
            if (g_ubrules[i].npus[j] == npu_id) {
                isNpuInRule = 1;
                break;
            }
        }
        if (isNpuInRule == 0) {
            continue;
        }
        if (g_ubrules[i].level != level) {
            continue;
        }
        if (mainBoardId == g_ubrules[i].mainboardId && die == g_ubrules[i].dieId && feId == g_ubrules[i].ueId) {
            return &g_ubrules[i];
        }
    }
    return NULL;
}

static int ProcessLayerClos(int npu_id, unsigned int mainBoardId, NetLayer *layer, UEList *ueList, struct dcmi_spod_info *spod_info)
{
    char net_instance_id[MAX_INSTANCE_ID_LEN] = {0};
    sprintf_s(net_instance_id, sizeof(net_instance_id), "superpod_%ld", spod_info->super_pod_id);
    NetLayerInit(layer, PRODUCT_CLOS_LEVEL, net_instance_id);
    NetLayerSetNetType(layer, NET_TYPE_CLOS);

    for (unsigned int i = 0; i < ueList->ueNum; ++i) {
        int fe = UBEntityGetId(&ueList->ueList[i]);
        int portGroupIdx = UBEntityGetServerPortGroupIdx(&ueList->ueList[i]);
        int die = UrmaEidGetPodDieId(&ueList->ueList[i].eidList[portGroupIdx].eid);
        if (portGroupIdx < 0) {
            continue;
        }
        const UBEntityRule *rule =  GetUBRule((npu_id % NPU_NUM), PRODUCT_CLOS_LEVEL, mainBoardId, die, fe);
        if (rule == NULL) {
            continue;
        }
        Addr addr;
        memset_s(&addr, sizeof(Addr), 0x00, sizeof(Addr));
        AddrSetEID(&addr, &ueList->ueList[i].eidList[portGroupIdx].eid);

        for (int j = 0; j < rule->portNum; ++j) {
            char port[MAX_PORT_LEN] = {0};
            sprintf_s(port, MAX_PORT_LEN, "%d/%d", die, rule->ports[j]);
            AddrAddPort(&addr, port);
        }
        AddrSetPlaneId(&addr, rule->planeId);
        NetLayerAddAddr(layer, &addr);
    }
    return 0;
}
    
int PodGetRootinfo(int npu_id, unsigned mainboard_id, void *buf, size_t *len)
{
    if (buf == NULL || len == NULL) {
        return RET_NOK;
    }
    RootInfo rootinfo;
    Rank rank;
    NetLayer layerMesh;
    NetLayer layerClos;
    RootInfoInit(&rootinfo);
    
    TopoGetFilePath(mainboard_id, rootinfo.topo_file_path, MAX_TOPO_PATH_LEN);
    struct dcmi_spod_info spod_info;
    UEList ueList;
    HalGetUBEntityList(npu_id, &ueList);
    hal_get_spod_info(npu_id, &spod_info);
    int localId = (npu_id % NPU_NUM) + ((spod_info.server_index % NPU_NUM) * NPU_NUM);
    RankInit(&rank, npu_id, localId);

    if (ProcessLayerMesh(npu_id, &layerMesh, &ueList, &spod_info) == 0) {
        RankAddNetLayer(&rank, &layerMesh);
    }
    if (ProcessLayerClos(npu_id, mainboard_id, &layerClos, &ueList, &spod_info) == 0) {
        RankAddNetLayer(&rank, &layerClos);
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
