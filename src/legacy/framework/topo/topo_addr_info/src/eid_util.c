/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "eid_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "securec.h"

#define UE_ID_POS (14)
#define HEX_BASE (16)
#define PORT_POS (30)
#define DIE_POS (39)
#define DIE_OFFSET (3)

static int Char2Int(char c, unsigned char * i)
{
    if (!isxdigit(c)) {
        return -1;
    }
    if (!isdigit(c)) {
        return -1;
    }
    *i = toupper(c) - '0';
    return 0;
}

int EidGetFeId(const char *eidhexstr)
{
    char fe = eidhexstr[UE_ID_POS];
    int fe_id = (int)strtol(&fe, NULL, HEX_BASE) >> 1;
    return fe_id;
}

int EidGetPortId(const char *eidhexstr, int *port_id)
{
    unsigned char end = (unsigned char)strtoul(&eidhexstr[PORT_POS], NULL, HEX_BASE);
    unsigned int flag = 0x80;
    unsigned int tmp = (~flag & end);
    unsigned int port = tmp >> 3;
    *port_id = (int)port;
    return 0;
}

int EidGetDieId(const char* eidhexstr, int *die_id)
{
    unsigned char d = 0;
    Char2Int(eidhexstr[DIE_POS], &d);
    *die_id = (int)((8 & d) >> DIE_OFFSET);
    return 0;
}

int UrmaEidGetFeId(dcmi_urma_eid_t *eid)
{
#define FE_POS (7)
#define FE_OFFSET (5)
    if (eid == NULL) {
        return -1;
    }
    int fe = eid->raw[FE_POS];
    fe = fe >> FE_OFFSET;
    return fe;
}

int UrmaEidGetPortId(dcmi_urma_eid_t *eid)
{
#define EID_PORT_LEFT_OFFSET (1)
#define EID_PORT_RIGHT_OFFSET (4)
    unsigned char last = eid->raw[DCMI_URMA_EID_SIZE - 1];
    last = last << EID_PORT_LEFT_OFFSET;
    last = last >> EID_PORT_RIGHT_OFFSET;
    return (int)last;
}

int UrmaEidGetDieId(dcmi_urma_eid_t *eid)
{
    unsigned char last = eid->raw[DCMI_URMA_EID_SIZE - 1];
    int die_id = (4 & last) == 0 ? 0 : 1;
    return die_id;
}

int GetMaxFeId(dcmi_urma_eid_info_t *eidList, size_t eidCnt)
{
    int maxFeId = -1;
    for (size_t i = 0; i < eidCnt; ++i) {
        int feId = UrmaEidGetFeId(&eidList[i].eid);
        if (feId > maxFeId) {
            maxFeId = feId;
        }
    }
    return maxFeId;
}

int UrmaEidGetLowBitPort(dcmi_urma_eid_t *eid)
{
    unsigned short lower = eid->raw[DCMI_URMA_EID_SIZE - 1];
    int port = lower & 0x7F;
    return port;
}

int UrmaEidGetServerDieId(dcmi_urma_eid_t *eid)
{
    const int dieIdOffset = 7;
    unsigned char bit = eid->raw[DCMI_URMA_EID_SIZE - 1];
    int dieId = bit >> dieIdOffset;
    return dieId;
}

int UBEntityGetId(UBEntity *ue)
{
    if (ue->eidNum == 0) {
        return -1;
    }
    int fe = UrmaEidGetFeId(&ue->eidList[0].eid);
    return fe;
}

#define MAX_PHY_PORT_ID (9)
int UBEntityGetServerPortGroupIdx(UBEntity *ue)
{
    for (int i = 0; i < (int)ue->eidNum; ++i) {
        int port = UrmaEidGetPortId(&ue->eidList[i].eid);
        if (port <= MAX_PHY_PORT_ID) {
            // 物理ID， 本函数取portgroup 序号
            continue;
        }
        return i;
    }
    // 没有找到
    return -1;
}

int UBGetMaxEntityId(UEList *ueList)
{
    int maxId = -1;
    for (unsigned int i = 0; i < ueList->ueNum; ++i) {
        if (ueList->ueList[i].eidNum == 0) {
            continue;
        }
        // 一个UBEntity下的所有eid的Entity ID相同
        int id = UrmaEidGetFeId(&ueList->ueList[i].eidList[0].eid);
        if (id > maxId) {
            maxId = id;
        }
    }
    return maxId;
}
