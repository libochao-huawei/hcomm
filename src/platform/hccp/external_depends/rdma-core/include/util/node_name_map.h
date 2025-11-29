/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __LIBUTIL_NODE_NAME_MAP_H__
#define __LIBUTIL_NODE_NAME_MAP_H__

#include <stdint.h>

struct nn_map;
typedef struct nn_map nn_map_t;

nn_map_t *open_node_name_map(const char *node_name_map);
void close_node_name_map(nn_map_t *map);
/* NOTE: parameter "nodedesc" may be modified here. */
char *remap_node_name(nn_map_t *map, uint64_t target_guid, char *nodedesc);
char *clean_nodedesc(char *nodedesc);

#endif
