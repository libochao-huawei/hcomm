/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __EID_UTIL_H__
#define __EID_UTIL_H__

#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 用于获取EID中的信息
 * EID是一个128bit的应用层地址
 * EID的格式说明:
 * |127:92|91:   78|77|76:74|73:      69|68:53|52   |51:32|31:0|
 * |无效  |超节点ID|  |无效 |UBEntity ID|无效 |固定1|无效 |CNA |
 * UBEntity ID 定义了本UBEntity的用途
 * CNA用于路由
 * CNA的格式定义如下
 * |31:   12|11   |10:            8|7      |6:   3|2:   0|
 * |固定前缀|固定0|subserverid(0-7)|iodie号|端口号|NPU ID|
 */

/**
 * 从EID的16进制字符串中解析出UBEntityID
 */
int EidGetFeId(const char *eid_str);

/**
 * 获取低3-6bit表示的物理端口号,  012bit表示NPU号
 */
int EidGetPortId(const char *eid_str, int* port_id);
int EidGetDieId(const char *eid_str, int* die_id);

int UrmaEidGetFeId(dcmi_urma_eid_t *eid);
int UrmaEidGetPortId(dcmi_urma_eid_t *eid);
int UrmaEidGetDieId(dcmi_urma_eid_t *eid);

/**
 * 获取低6bit表示的逻辑端口号
 */
int UrmaEidGetLowBitPort(dcmi_urma_eid_t *eid);

/**
 * 获取FE ID, FE是UB中的功能实体, 在EID编址规则中，讲FE ID编在了EID中
 */
int EidGetFeId(const char *eidhexstr);

/**
 * 根据EID编址规范，FE最大的是mesh链接使用
 */
int GetMaxFeId(dcmi_urma_eid_info_t *eidList, size_t eid_cnt);

int UBEntityGetId(UBEntity *ue);

int UBEntityGetServerPortGroupIdx(UBEntity *ue);

int UrmaEidGetServerDieId(dcmi_urma_eid_t *eid);

int UBGetMaxEntityId(UEList *ueList);

#ifdef __cplusplus
}
#endif

#endif // __EID_UTIL_H__
