/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCE_LOG_H
#define HCCE_LOG_H

#include <cinttypes>
#include <cstdint>
#include "external/ge_common/ge_api_types.h"
#include "external/ge_common/ge_api_error_codes.h"
#include "common/ge_common/debug/log.h"
#include "common/ge_common/debug/ge_log.h"
#include "common/ge_common/ge_inner_error_codes.h"
#include "common/util/error_manager/error_manager.h"
#include "toolchain/slog.h"
#include "common/checker.h"
#ifdef __GNUC__
#include <unistd.h>
#include <sys/syscall.h>
#else
#include "mmpa/mmpa_api.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GE_MODULE_NAME static_cast<int32_t>(GE)

#define HCCE_ERROR(ERROR_CODE, fmt, ...)                                                                \
  do {                                                                                              \
    dlog_error(GE_MODULE_NAME, "%" PRIu64 " %s: ErrorNo: %" PRIuLEAST8 "(%s) %s [HCCE]" fmt, \
	       GeLog::GetTid(), &__FUNCTION__[0U], \
               (ERROR_CODE), ((GE_GET_ERRORNO_STR(ERROR_CODE)).c_str()),                            \
               ErrorManager::GetInstance().GetLogHeader().c_str(), ##__VA_ARGS__);                  \
  } while (false)

#define HCCE_WARNING(fmt, ...)                                                                          \
  do {                                                                                            \
    dlog_warn(GE_MODULE_NAME, "%" PRIu64 " %s: [HCCE]" fmt, GeLog::GetTid(), &__FUNCTION__[0U], ##__VA_ARGS__); \
  } while (false)

#define HCCE_INFO(fmt, ...)                                                                          \
  do {                                                                                            \
    dlog_info(GE_MODULE_NAME, "%" PRIu64 " %s: [HCCE]" fmt, GeLog::GetTid(), &__FUNCTION__[0U], ##__VA_ARGS__); \
  } while (false)

#define HCCE_DEBUG(fmt, ...)                                                                           \
  do {                                                                                             \
    dlog_debug(GE_MODULE_NAME, "%" PRIu64 " %s: [HCCE]" fmt, GeLog::GetTid(), &__FUNCTION__[0U], ##__VA_ARGS__); \
  } while (false)

#define HCCE_RUN_INFO(fmt, ...)                                                                        \
  do {                                                                                                               \
    dlog_info(static_cast<int32_t>(static_cast<uint32_t>(RUN_LOG_MASK) | static_cast<uint32_t>(GE_MODULE_NAME)),     \
        "%" PRIu64 " %s: [HCCE]" fmt, GeLog::GetTid(), &__FUNCTION__[0U], ##__VA_ARGS__);                            \
    if (!IsLogPrintStdout()) {                                                \
      dlog_info(GE_MODULE_NAME, "%" PRIu64 " %s:" fmt, GeLog::GetTid(), &__FUNCTION__[0U], ##__VA_ARGS__); \
    }                                                                                            \
  } while (false)

#ifndef LIKELY
#define LIKELY(x) (static_cast<bool>(__builtin_expect(static_cast<bool>(x), 1)))
#define UNLIKELY(x) (static_cast<bool>(__builtin_expect(static_cast<bool>(x), 0)))
#endif

/* 检查函数返回值, 并返回指定错误码 */
#define HCCE_CHK_RET(expr)                  \
  do {                                      \
    const ge::Status _chk_status = (expr);  \
    if (_chk_status != ge::SUCCESS) {       \
      return _chk_status;                   \
    }                                       \
  } while (false)

/* 检查函数返回值, 记录指定日志, 并返回指定错误码 */
#define HCCE_CHK_PRT_RET(result, exeLog, retCode) \
  do {                                      \
      if (UNLIKELY(result)) {               \
          exeLog;                           \
          return retCode;                   \
      }                                     \
  } while (false)

// Check if the parameter is null. If yes, return PARAM_INVALID and record the error
#define HCCE_CHECK_NOTNULL(val, ...)                                                          \
  do {                                                                                      \
    if ((val) == nullptr) {                                                                 \
      REPORT_INNER_ERROR("E19999", "Param:" #val " is nullptr, check invalid" __VA_ARGS__); \
      HCCE_ERROR(ge::FAILED, "[Check][Param:" #val "]null is invalid" __VA_ARGS__);             \
      return ge::PARAM_INVALID;                                                             \
    }                                                                                       \
  } while (false)

// If expr is not SUCCESS, print the log and do not execute return
#define HCCE_CHK_PRT(expr, ...)            \
  do {                                      \
    const ge::Status _chk_status = (expr);  \
    if (_chk_status != ge::SUCCESS) {       \
      HCCE_ERROR(_chk_status, __VA_ARGS__);     \
    }                                       \
  } while (false)

#ifdef __cplusplus
}
#endif
#endif  // HCCE_LOG_H
