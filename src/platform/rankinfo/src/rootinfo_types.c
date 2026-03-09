/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rootinfo_types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "eid_util.h"
#include "hal.h"

#define MAX_PORTS_STR_LEN (512)


char* AddrToString(const Addr* addr)
{
    const size_t max_buffer_size = 102400;
    char* buf = (char*)malloc(max_buffer_size);
    if(buf == NULL) {
        return NULL;
    }
    hal_memset_s(buf, max_buffer_size, 0, max_buffer_size);
    char ports[MAX_PORTS_STR_LEN] = {0};
    for(int i = 0; i < addr->port_count; i++) {
        if(i > (MAX_PORT_NUM - 1)) {
            break;
        }
        char port[MAX_PORT_LEN] = {0};
        hal_sprintf_s(port, sizeof(port), "\"%s\"" , addr->ports[i]);
        hal_strcat_s(ports, MAX_PORTS_STR_LEN, port);
        if (i != addr->port_count - 1) {
            hal_strcat_s(ports, MAX_PORTS_STR_LEN, ",");
        }
    }
    hal_sprintf_s(buf, max_buffer_size,
        "{\"addr_type\": \"%s\", \"addr\": \"%s\", \"plane_id\": \"%s\", \"ports\": [%s]}",
        addr->addr_type, addr->addr, addr->plane_id, ports);
    return buf;
}

void NetLayerInit(NetLayer *layer, int level, const char* layer_id)
{
    hal_memset_s(layer, sizeof(NetLayer), 0, sizeof(NetLayer));
    layer->net_layer = level;
    layer->addr_count = 0;
    hal_strcpy_s(layer->net_instance_id, sizeof(layer->net_instance_id), layer_id);
}

void NetLayerSetNetType(NetLayer *layer, const char* net_type)
{
    hal_strcpy_s(layer->net_type, sizeof(layer->net_type), net_type);
}

void NetLayerAddAddr(NetLayer *layer, const Addr *addr)
{
    if(layer->addr_count >= MAX_ADDR_NUM) {
        return;
    }
    hal_memcpy_s(&layer->rank_addr_list[layer->addr_count], sizeof(Addr), addr, sizeof(Addr));
    layer->addr_count++;
}

char* NetLayerToString(const NetLayer *layer)
{
    const size_t max_buffer_size = 102400;
    char* buf = (char*)malloc(max_buffer_size);
    if(buf == NULL) {
        return NULL;
    }
    char* addr_list = (char*)malloc(max_buffer_size);
    if(addr_list == NULL) {
        return NULL;
    }
    hal_memset_s(addr_list, max_buffer_size, 0, max_buffer_size);
    hal_memset_s(buf, max_buffer_size, 0, max_buffer_size);
    for(int i = 0; i < layer->addr_count; i++) {
        char *addr = AddrToString(&layer->rank_addr_list[i]);
        if(addr == NULL) {
            continue;
        }
        hal_strcat_s(addr_list, max_buffer_size, addr);
        if (i != layer->addr_count - 1) {
            hal_strcat_s(addr_list, max_buffer_size, ",");
        }
        free(addr);
    }
    hal_sprintf_s(buf, max_buffer_size,
        "{\"net_layer\": %d, \"net_instance_id\": \"%s\", \"net_type\": \"%s\",\"net_attr\":\"%s\","
        "\"rank_addr_list\": [%s]}",
        layer->net_layer, layer->net_instance_id, layer->net_type, layer->net_attr, addr_list);
    free(addr_list);
    return buf;
}

char* RankListToString(const RootInfo *rootinfo)
{
    const size_t max_buffer_size = 102400;
    char* buf = (char*)malloc(max_buffer_size);
    if(buf == NULL) {
        return NULL;
    }
    hal_memset_s(buf, max_buffer_size, 0, max_buffer_size);
    for(int i = 0; i < rootinfo->rank_count; i++) {
        char *rank = RankToString(&rootinfo->ranks[i]);
        hal_strcat_s(buf, max_buffer_size, rank);
        if (i != rootinfo->rank_count - 1) {
            hal_strcat_s(buf, max_buffer_size, ",");
        }
        free(rank);
    }
    return buf;
}

void RankInit(Rank *rank, int deviceId, int localId)
{
    hal_memset_s(rank, sizeof(Rank), 0, sizeof(Rank));
    rank->device_id = deviceId;
    rank->local_id = localId;
    rank->level_count = 0;
}

void RankAddNetLayer(Rank *rank, const NetLayer *layer)
{
    if(rank->level_count >= MAX_NET_LEVEL_NUM) {
        return;
    }
    hal_memcpy_s(&rank->level_list[rank->level_count], sizeof(NetLayer), layer, sizeof(NetLayer));
    rank->level_count++;
}

char *RootInfoToString(const RootInfo *rootinfo)
{
    const size_t max_buffer_size = 102400;
    char* buf = (char*)malloc(max_buffer_size);
    if(buf == NULL) {
        return NULL;
    }
    hal_memset_s(buf, max_buffer_size, 0, max_buffer_size);
    char *rank_list = RankListToString(rootinfo);
    hal_sprintf_s(buf, max_buffer_size,
        "{\"version\": \"%s\", \"topo_file_path\": \"%s\","
        "\"rank_count\": %d, \"rank_list\": [%s]}",
        rootinfo->version, rootinfo->topo_file_path, rootinfo->rank_count, rank_list);
    free(rank_list);
    return buf;
}

void RootInfoInit(RootInfo *rootinfo)
{
    hal_memset_s(rootinfo, sizeof(RootInfo), 0, sizeof(RootInfo));
    hal_strcpy_s(rootinfo->version, sizeof(rootinfo->version), "2.0");
    rootinfo->rank_count = 0;
}

void RootInfoAddRank(RootInfo *rootinfo, const Rank *rank)
{
    if(rootinfo->rank_count >= MAX_RANK_NUM) {
        return;
    }
    hal_memcpy_s(&rootinfo->ranks[rootinfo->rank_count], sizeof(Rank), rank, sizeof(Rank));
    rootinfo->rank_count++;
}

void AddrInit(Addr *addr)
{
    hal_memset_s(addr, sizeof(Addr), 0, sizeof(Addr));
}

void AddrSetEID(Addr *addr, const dcmi_urma_eid_t *eid)
{
    for (int i = 0; i < DCMI_URMA_EID_SIZE; i++) {
        hal_sprintf_s(&addr->addr[i*2], 3, "%02x", eid->raw[i]);
    }
    hal_strcpy_s(addr->addr_type, sizeof(addr->addr_type), "EID");
}

void AddrSetIP(Addr* addr, const char* ip)
{
    hal_strcpy_s(addr->addr, sizeof(addr->addr), ip);
    hal_strcpy_s(addr->addr_type, sizeof(addr->addr_type), "IPV4");
}

void AddrSetPlaneId(Addr *addr, const char* plane_id)
{
    hal_strcpy_s(addr->plane_id, sizeof(addr->plane_id), plane_id);
}

int AddrAddPort(Addr *addr, const char* port)
{
    if (addr->port_count >= (MAX_PORT_NUM - 1)) {
        return -1;
    }
    hal_strcpy_s(addr->ports[addr->port_count++], MAX_PORT_LEN, port);
    return 0;
}

char* RankToString(const Rank* rank)
{
    const size_t max_buf_size = 102400;
    char *buf = (char*)malloc(max_buf_size);
    char* level_list = (char*)malloc(max_buf_size);
    for (int i = 0; i < rank->level_count; ++i) {
        char *layer = NetLayerToString(&rank->level_list[i]);
        (void)hal_strcat_s(level_list, max_buf_size, layer);
        if (i != (rank->level_count - 1)) {
            (void)hal_strcat_s(level_list, max_buf_size, ", ");
        }
        free(layer);
    }
    hal_sprintf_s(buf, max_buf_size, "{\"device_id\": %d, \"local_id\": %d, \"level_list\": [%s]}",
                  rank->device_id, rank->local_id, level_list);
    free(level_list);
    return buf;
}
