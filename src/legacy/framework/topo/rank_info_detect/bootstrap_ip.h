/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: get bootstrap ip declaration
 * Create: 2025-09-16
 */

#ifndef HCCL_BOOTSTRAP_IP_H
#define HCCL_BOOTSTRAP_IP_H

#include <unordered_map>
#include "orion_adapter_hccp.h"
#include "ip_address.h"
#include "universal_concurrent_map.h"

namespace Hccl {

// 获取bootstrapIp
const IpAddress &GetBootstrapIp(u32 devPhyId);

} // namespace Hccl

#endif // HCCL_BOOTSTRAP_IP_H