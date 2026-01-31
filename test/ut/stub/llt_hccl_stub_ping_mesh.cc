/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include <thread>
#include <chrono>
#include <securec.h>
#include <gtest/gtest.h>


//#define private public
#include "hccl_network_pub.h"
#include "network_manager_pub.h"
#include "ping_mesh.h"
//#undef private

#include "adapter_hccp.h"
#include "adapter_tdt.h"
#include "adapter_hal.h"
#include "adapter_rts.h"
#include "adapter_rts_common.h"
#include "hdc_pub.h"
#include "profiler_base_pub.h"
#include "tsd/tsd_client.h"
#include "network/hccp_common.h"
#include "network/hccp_ping.h"
#include "externalinput.h"
#ifdef CONFIG_CONTEXT
#include "hccp_ctx.h"
#endif
#include "local_ub_rma_buffer.h"
#include "orion_adapter_hccp.h"


namespace Hccl {
    u32 GetUbToken()
    {
        return 0;
    }

}

 int ra_get_dev_eid_info_num(struct RaInfo info, unsigned int *num)
{

    return 0;
}

int ra_get_dev_eid_info_list(struct RaInfo info, struct dev_eid_info info_list[],
    unsigned int *num)
{

    return 0;
}
extern "C" {
    extern HcclResult RaGetEidMap(std::map<hccl::Eid, uint32_t>& eidmap, const hccl::HRaInfo &raInfo)
{
    eidmap.clear();  // 清空输出map

    // 基础EID字符串（去除最后两位）
    const std::string hex_str = "000000000000002000100000dfdf1700";


    // 循环插入尾数00-09
    for (uint32_t i = 0; i < 11; ++i) {
        // 生成当前尾数（两位十六进制小写）
        std::string modified_str = hex_str.substr(0, hex_str.length() - 1);
        std::string input = modified_str + std::to_string(i);      
        // 转换为Eid类型并插入map
        hccl::Eid eid = hccl::HcclIpAddress::StrToEID(input);
        eidmap.emplace(eid, i);  // 插入对应值i
    }

    return HCCL_SUCCESS;
}
}




