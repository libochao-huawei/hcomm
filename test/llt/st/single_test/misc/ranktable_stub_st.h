/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RANKTABLE_UT_STUB_ST_H_
#define RANKTABLE_UT_STUB_ST_H_
 
#include <string>
const std::string rankTable_ut_stub_4p = R"(
    {
        "version": "2.0",
        "rank_count" : "4",
        "rank_list": [
            {
                "rank_id": 0,
                "local_id": 0,
                "level_list":  [
                    {
                        "level": 0,
                        "id" : "az0-rack0", 
                        "fabric_type": "INNER", 
                        "rank_addr_type": "",
                        "rank_addrs": []
                    }
                ]
            },
            {
                "rank_id": 1,
                "local_id": 1,
                "level_list":  [
                    {
                        "level": 0,
                        "id" : "az0-rack0", 
                        "fabric_type": "INNER", 
                        "rank_addr_type": "",
                        "rank_addrs": []
                    }
                ]
            },
            {
                "rank_id": 2,
                "local_id": 2,
                "level_list":  [
                    {
                        "level": 0,
                        "id" : "az0-rack0", 
                        "fabric_type": "INNER", 
                        "rank_addr_type": "",
                        "rank_addrs": []
                    }
                ]
            },
            {
                "rank_id": 3,
                "local_id": 3,
                "level_list":  [
                    {
                        "level": 0,
                        "id" : "az0-rack0", 
                        "fabric_type": "INNER", 
                        "rank_addr_type": "",
                        "rank_addrs": []
                    }
                ]
            }
        ],
 
        "replace_count" : 1,
        "replace_list" : [ 
            {"level": 0,  "group_id" : "az0-rack0", "backup_local_id": 64, "target_local_id": 1}
        ]
    }
)";
 
#endif
