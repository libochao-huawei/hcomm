/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HSS_TYPES_H
#define HSS_TYPES_H

namespace Hss {
/**
 * @brief HSS Status
 */

using handle = void*;

enum class HssStatus {
    HSS_SUCCESS = 0,               /**< success */
    HSS_E_INTERNAL = 1,            /**< internal error */
    HSS_E_IO,                      /**< io error */
    HSS_E_NOMEM,                   /**< no memory error */
    HSS_E_SHUTDOWN,                /**< shut down error */
    HSS_FILT_FAIL,
    HSS_FILT_MBLOCK,
    HSS_E_RESERVED                 /**< reserved */
};

enum class Role {
    SERVER = 0,
    CLIENT = 1,
    INVALID_ROLE
};

enum class NetworkMode {
    NETWORK_PEER_ONLINE = 0, /**< Third-party online mode */
    NETWORK_OFFLINE, /**< offline mode */
    NETWORK_ONLINE, /**< online mode */
};

using s8 = signed char;
using u8 = unsigned char;
using s16 = signed short;
using u16 = unsigned short;
using s32 = signed int;
using u32 = unsigned int;
using s64 = signed long long;
using u64 = unsigned long long;
using f32 = float;
using d64 = double;

constexpr s64 LONG_NEGATIVE_MAX = 0x8000000000000000LL;
constexpr s64 LONG_NEGATIVE_ONE = -1LL;

}

#endif