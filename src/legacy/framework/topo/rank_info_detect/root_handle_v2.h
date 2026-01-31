/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: HcclRootHandleV2 struct header file
 * Create: 2025-09-16
 */

#ifndef HCCL_ROOT_HANDLE_V2_H
#define HCCL_ROOT_HANDLE_V2_H

#include "orion_adapter_hccp.h"
#include "ip_address.h"
#include "hccl_common_v2.h"

namespace Hccl {

// HcclRootHandleV2 定义
constexpr u32     IP_ADDRESS_BUFFER_LEN           = 64;
const std::string RANK_INFO_DETECT_TAG            = "rank_info_detect_default_tag";

using HcclRootHandleV2 = struct HcclRootHandleDefV2 {
    char         ip[IP_ADDRESS_BUFFER_LEN];
    u32          listenPort{HCCL_INVALID_PORT};
    HrtNetworkMode netMode{HrtNetworkMode::HDC};
    char         identifier[ROOTINFO_INDENTIFIER_MAX_LENGTH];
};

// buffer大小
constexpr u32 MAX_BUFFER_LEN  = 10 * 1024 * 1024; // 10M

constexpr u32 ONE_MILLISECOND_OF_USLEEP = 1000;

} // namespace Hccl

#endif // HCCL_ROOT_HANDLE_V2_H