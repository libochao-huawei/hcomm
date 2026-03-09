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
#ifdef __cplusplus
extern "C" {
#endif
#include "hal.h"

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
