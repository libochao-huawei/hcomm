/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rankinfo.h"
#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include "hal.h"
#include "topo.h"
#include "product_card.h"
#include "product_server.h"

int GetRankInfoSize(int phyId, size_t* size)
{
    if (size == NULL) {
        return -1;
    }
    uint32_t mainboard_id = 0;
    int ret = hal_get_mainboard_id(phyId, &mainboard_id);
    if (ret != 0) {
        return ret;
    }
    if (mainboard_id == MAIN_BOARD_ID_CARD_4PMESH) {
        return GetCardRankInfoLen(size);
    }
    if (mainboard_id == MAIN_BOARD_ID_SERVER_8PMESH) {
        return ServerGetRootinfoLen(size);
    }
    return 0;
}

static int PassThroughTopoFilePath(char* filePath, size_t bufSize)
{
    return GetTopoFilePathFromFile("/etc/hccl_rootinfo.json", filePath, bufSize);
}

/**
 *  获取拓扑文件路径
 */
int GetTopoFilePath(int phyId, char* filePath, size_t bufSize)
{
    if (filePath == NULL) {
        return -1;
    }
    // 优先从/etc/hccl_rootinfo.json中读取
    int ret = PassThroughTopoFilePath(filePath, bufSize);
    if (ret == 0) {
        return ret;
    }
    uint32_t mainboard_id = 0;
    ret = hal_get_mainboard_id(phyId, &mainboard_id);
    if (ret != 0) {
        return ret;
    }
    return TopoGetFilePath(mainboard_id, filePath, bufSize);
}

static void DumpFile(char* rankInfo, size_t bufSize)
{
    char* dumpFile = getenv("RANKINFO_DUMP_FILE");
    if (dumpFile != NULL) {
        FILE* fp = fopen(dumpFile, "w+");
        if (fp == NULL) {
            return;
        }
        int ret = fwrite(rankInfo, 1, bufSize, fp);
        if (ret < 0) {
            fclose(fp);
            return;
        }
        fclose(fp);
    }
}

static int PassThrough(char *rankInfo, size_t *bufSize)
{
    FILE *fp = fopen("/etc/hccl_rootinfo.json", "r");
    if (fp == NULL) {
        return -1;
    }
    struct stat stat;
    fstat(fileno(fp), &stat);
    if ((size_t)stat.st_size > (*bufSize)) {
        fclose(fp);
        return -1;
    }
    int ret = fread(rankInfo, 1, stat.st_size, fp);
    if (ret < 0) {
        fclose(fp);
        return -1;
    }
    *bufSize = (size_t)stat.st_size;
    return 0;
}

int GetDeviceRankInfo(int phyId, char* rankInfo, size_t *bufSize)
{
    if (rankInfo == NULL || bufSize == NULL) {
        return -1;
    }
    // 优先读取/etc/hccl_rootinfo.json中内容
    if (PassThrough(rankInfo, bufSize) == 0) {
        return 0;
    }
    // 若/etc/hccl_rootinfo.json中无内容，根据mainboard_id生成rootinfo
    uint32_t mainboard_id = 0;
    int ret = hal_get_mainboard_id(phyId, &mainboard_id);
    if (ret != 0) {
        return ret;
    }
    if ((mainboard_id == MAIN_BOARD_ID_CARD_4PMESH)
      ||(mainboard_id ==  MAIN_BOARD_ID_CARD_NOMESH)) {
        ret = GetCardRankInfo(phyId, mainboard_id, rankInfo, bufSize);
    }
    if ((mainboard_id == MAIN_BOARD_ID_SERVER_8PMESH)
      ||(mainboard_id == MAIN_BOARD_ID_SERVER_TYPE1)) {
        ret = ServerGetRootinfo(phyId, mainboard_id, rankInfo, bufSize);
    }
    DumpFile(rankInfo, *bufSize);
    return 0;
}
