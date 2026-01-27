/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#ifndef HCCL_AIV_UTILS_H
#define HCCL_AIV_UTILS_H
 
#include "string"
 
#include "hccl_types.h"
#include "orion_adapter_rts.h"
#include "template_utils.h"
#include "acl/acl_rt.h"
 
namespace Hccl {
constexpr u32 MAX_RANK_SIZE_ = 8; // 注意要和device侧的一致
constexpr u32 MAX_BLOCK_DIM = 56; // 56-72
 
constexpr s32 TAG_INIT_VALUE = 1;
constexpr s32 TAG_RESET_COUNT = 1000;
constexpr s32 TOPO_LEN = 32;
 
constexpr u32 AIV_TAG_MOVE_LEFT_BITS = 16;
constexpr u32 AIV_TAG_ADDR_OFFSET = 16 * 1024;
constexpr u32 AIV_TOPO_ADDR_OFFSET = 32 * 1024;
constexpr u32 AIV_TOPO_BUFF_LEN = 8 * 1024;
constexpr u32 AIV_FLAG_ADDR_OFFSET = 40 * 1024;
constexpr u32 AIV_FLAG_AREA_SIZE = 1000 * 1024;
constexpr u32 AIV_FLAG_CLEAR_OFFSET = 1040 * 1024;
constexpr u32 AIV_TAG_BUFF_LEN = 2 * 1024 * 1024;
constexpr u32 AIV_LOW_16_BITS = 0xFFFF;

constexpr u32 CACHEMAP_MAXSIZE = 65536;
constexpr float CACHEMAP_CLEARPERCENT = 0.1;
 
enum class KernelArgsType {
    ARGS_TYPE_SERVER = 0, // kernel参数为单机内
    ARGS_TYPE_TWO_SHOT = 1,
    ARGS_TYPE_DEFAULT
};
 
struct all2AllDataDes {
    DataType sendType;
    DataType recvType;
    u64 sendCount;
    u64 recvCount;
    // 重载 != 操作符
    bool operator!=(const all2AllDataDes& other) const {
        return sendType != other.sendType || 
               recvType != other.recvType || 
               sendCount != other.sendCount || 
               recvCount != other.recvCount;
    }
 
    // 重载 < 操作符
    bool operator<(const all2AllDataDes& other) const {
        if (sendType != other.sendType) return sendType < other.sendType;
        if (recvType != other.recvType) return recvType < other.recvType;
        if (sendCount != other.sendCount) return sendCount < other.sendCount;
        return recvCount < other.recvCount;
    }
};
 
struct all2AllVDataDes{
    DataType sendType;
    DataType recvType;
    void* sendCounts;
    void* recvCounts;
    void* sdispls;
    void* rdispls;
        // 重载 != 操作符
    bool operator!=(const all2AllVDataDes& other) const {
        return sendType != other.sendType || 
               recvType != other.recvType || 
               sendCounts != other.sendCounts || 
               recvCounts != other.recvCounts || 
               sdispls != other.sdispls || 
               rdispls != other.rdispls;
    }
 
    // 重载 < 操作符
    bool operator<(const all2AllVDataDes& other) const {
        if (sendType != other.sendType) return sendType < other.sendType;
        if (recvType != other.recvType) return recvType < other.recvType;
        if (sendCounts != other.sendCounts) return sendCounts < other.sendCounts;
        if (recvCounts != other.recvCounts) return recvCounts < other.recvCounts;
        if (sdispls != other.sdispls) return sdispls < other.sdispls;
        return rdispls < other.rdispls;
    }
};
 
// 表示算子属性的参数，相对固定
struct AivOpCacheArgs {
    std::string algName = "default";
    u64 count = 0;
    DataType dataType = DataType::INT32; 
    uint16_t opType = 0;
    ReduceOp reduceOp = ReduceOp::INVALID;
    u32 root = 0;
    u32 blockDimLimit = 0;
    DataType outputDataType = DataType::INT32; 
    all2AllDataDes all2allDataDes{};
    all2AllVDataDes all2allVDataDes{};
 
    AivOpCacheArgs(
        const std::string& algName_,
        u64 count_,
        DataType dataType_,
        uint16_t opType_,
        ReduceOp reduceOp_,
        u32 root_,
        u32 blockDimLimit_,
        DataType outputDataType_,
        const all2AllDataDes& all2allDataDes_ = {},
        const all2AllVDataDes& all2allVDataDes_ = {}
    ) : algName(algName_),
        count(count_),
        dataType(dataType_),
        opType(opType_),
        reduceOp(reduceOp_),
        root(root_),
        blockDimLimit(blockDimLimit_),
        outputDataType(outputDataType_),
        all2allDataDes(all2allDataDes_),
        all2allVDataDes(all2allVDataDes_)
    {}
        // 自定义 operator<
    bool operator<(const AivOpCacheArgs& other) const {
        if (algName != other.algName) return algName < other.algName;
        if (count != other.count) return count < other.count;
        if (dataType != other.dataType) return dataType < other.dataType;
        if (opType != other.opType) return opType < other.opType;
        if (reduceOp != other.reduceOp) return reduceOp < other.reduceOp;
        if (root != other.root) return root < other.root;
        if (blockDimLimit != other.blockDimLimit) return blockDimLimit < other.blockDimLimit;
        if (all2allDataDes != other.all2allDataDes) return all2allDataDes < other.all2allDataDes;
        if (all2allVDataDes != other.all2allVDataDes) return all2allVDataDes < other.all2allVDataDes;
        return outputDataType < other.outputDataType;
    }
};
 