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

int ServerGetRootinfoLen(size_t *len)
{
    *len = MAX_SERVER_ROOTINFO_LEN;
    return 0;
}

#define MAX_PORT_NUM (32)
typedef struct rule {
    unsigned int mainboardId;
    int level;
    int dieId;
    int ueId;
    int ports[MAX_PORT_NUM];
    int portNum;
}UBEntityRule;

static const UBEntityRule g_ubrules[] = {
    {MAIN_BOARD_ID_SERVER_TYPE1, PRODUCT_CLOS_LEVEL, 0, 3, {4,5,6,7}, 4},
    {MAIN_BOARD_ID_SERVER_TYPE1, PRODUCT_CLOS_LEVEL, 1, 2, {5,6}, 2},
    //使用0 die上的fe 3出clos, fe3包含8个口
    {MAIN_BOARD_ID_SERVER_8PMESH, PRODUCT_CLOS_LEVEL, 0, 3, {1,2,3,4,5,6,7,8}, 8},
};


static int ProcessLayerMesh(NetLayer *layer, UEList *ueList, struct dcmi_spod_info *spod_info)
{
    NetLayerSetNetType(layer, NET_TYPE_MESH);
    char net_instance_id[MAX_INSTANCE_ID_LEN] = {0};
    sprintf_s(net_instance_id, sizeof(net_instance_id), "sp%ld_srv%ld", spod_info->super_pod_id, spod_info->server_index);
    NetLayerInit(layer, PRODUCT_MESH_LEVEL, net_instance_id);


    int meshEntityId = UBGetMaxEntityId(ueList);
    const unsigned minMeshEidNum = 7;
    const int meshDie = 1;
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
            char planeId[MAX_PLANE_ID_LEN] = {0};
            // topo中端口从0开始编，CNA中需要规避全0，从1开始
            sprintf_s(port, MAX_PORT_LEN, "%d/%d", meshDie, (phyPortId - 1));
            sprintf_s(planeId, sizeof(planeId), "plane_%d", meshDie);
            AddrAddPort(&addr, port);
            AddrSetPlaneId(&addr, planeId);
            NetLayerAddAddr(layer, &addr);
        }
    }
    return 0;
}

static int ProcessLayerClos(unsigned int mainBoardId, NetLayer *layer, UEList *ueList, struct dcmi_spod_info *spod_info)
{
    char net_instance_id[MAX_INSTANCE_ID_LEN] = {0};
    sprintf_s(net_instance_id, sizeof(net_instance_id), "superpod_%ld", spod_info->super_pod_id);
    NetLayerInit(layer, PRODUCT_CLOS_LEVEL, net_instance_id);
    NetLayerSetNetType(layer, NET_TYPE_CLOS);

    for (unsigned int i = 0; i < ueList->ueNum; ++i) {
        int fe = UBEntityGetId(&ueList->ueList[i]);
        int portGroupIdx = UBEntityGetServerPortGroupIdx(&ueList->ueList[i]);
        int die = UrmaEidGetServerDieId(&ueList->ueList[i].eidList[portGroupIdx].eid);
        if (portGroupIdx < 0) {
            continue;
        }
        for (size_t r = 0; r < sizeof(g_ubrules)/sizeof(UBEntityRule); ++r) {
            if (mainBoardId == g_ubrules[r].mainboardId && die == g_ubrules[r].dieId && fe == g_ubrules[r].ueId) {
                Addr addr;
                memset_s(&addr, sizeof(Addr), 0x00, sizeof(Addr));
                AddrSetEID(&addr, &ueList->ueList[i].eidList[portGroupIdx].eid);

                for (int j = 0; j < g_ubrules[r].portNum; ++j) {
                    char port[MAX_PORT_LEN] = {0};
                    sprintf_s(port, MAX_PORT_LEN, "%d/%d", die, g_ubrules[r].ports[j]);
                    AddrAddPort(&addr, port);
                }
                char planeId[MAX_PLANE_ID_LEN] = {0};
                sprintf_s(planeId, sizeof(planeId), "plane_%d", die);
                AddrSetPlaneId(&addr, planeId);
                NetLayerAddAddr(layer, &addr);
            }
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
    NetLayer layerMesh;
    NetLayer layerClos;
    RootInfoInit(&rootinfo);
    RankInit(&rank, npu_id, npu_id);
    
    TopoGetFilePath(mainboard_id, rootinfo.topo_file_path, MAX_TOPO_PATH_LEN);
    struct dcmi_spod_info spod_info;
    UEList ueList;
    HalGetUBEntityList(npu_id, &ueList);
    hal_get_spod_info(npu_id, &spod_info);

    if (ProcessLayerMesh(&layerMesh, &ueList, &spod_info) == 0) {
        RankAddNetLayer(&rank, &layerMesh);
    }
    if (ProcessLayerClos(mainboard_id, &layerClos, &ueList, &spod_info) == 0) {
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
