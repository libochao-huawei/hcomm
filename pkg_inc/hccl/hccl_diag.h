/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_DIAG_H
#define HCCL_DIAG_H

#include <cstddef>
#include <hccl/hccl_types.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
const u32 HCOMM_ALG_TAG_LENGTH = 288;
/**
 * @brief 注册算子信息的DFX接口
 * @param[in] comm 通信域句柄，标识当前通信上下文
 * @param[in] HcclDfxOpInfo 算子信息结构体，包含算子对象，通信操作标签名等
 * @return HcclResult 执行结果状态码
 * @note host侧
 */
extern HcclResult HcclDfxRegOpInfo(HcclComm comm, void* dfxOpInfo);
/**
 * @brief 算子上报性能数据（开始时间戳）
 * @param[in] beginTime 算子开始执行的时间戳
 * @return HcclResult 执行结果状态码
 * @note host侧
 */
extern HcclResult HcclProfilingReportOp(HcclComm comm, uint64_t beginTime);

/**
 * @brief kernel上报
 * @param[in] comm 通信域句柄，标识当前通信上下文
 * @param[in] beginTime 算子开始执行的时间戳
 * @param[in] thread 线程上下文
 * @return HcclResult 执行结果状态码
 * @note host侧
 */
extern HcclResult HcclReportAicpuKernel(HcclComm comm, uint64_t beginTime, char *kernelName);

extern uint64_t HcommGetProfilingSysCycleTime();


struct HcclDfxOpInfo {
    //DfxOpInfo_base
    u64                 beginTime{0};
    u64                 endTime{0};
    //CollOperator
    bool                staticAddr{false};
    bool                staticShape{false};
    //baseCollOperator
    u32                 opMode{0}; // 单算子和图模式
    u32                 opType{0}; // 算子名称类型
    u32                 reduceOp{0};
    u32                 dataType{0};
    u32                 outputType{0}; //暂不删除，考虑后续算子使用
    u64                 dataCount{0};
    u32                 root = INVALID_VALUE_RANKID;
    void*               inputMemPtr{nullptr};
    u64                 inputMemSize{0};
    void*               outputMemPtr{nullptr};
    u64                 outputMemSize{0};
    void*               scratchMemPtr{nullptr};
    u64                 scratchMemSize{0};
    //task_exception
    u64                 cpuTsThread{0}; // host侧算子主流的threadhandle
    u32                 cpuWaitAicpuNotifyIdx{0}; // host wait device notifyIdx
    u32                 cpuWaitAicpuNotifyId{0}; // host wait device notifyId
    char                algTag[HCOMM_ALG_TAG_LENGTH]{}; // 算法名 = "算子类型 + 通信域id + 选择的算法"
};

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif