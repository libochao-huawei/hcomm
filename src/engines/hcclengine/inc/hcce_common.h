/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCE_COMMON_H
#define HCCE_COMMON_H

#include "hccl/hcom.h"
#include "hccl/base.h"
#include "hccl/hccl.h"

constexpr u32 GROUP_NAME_MAX_LEN = 127; // 最大的group name 长度

/* 未使用的参数声明 */
constexpr u32 INVALID_UINT = 0xFFFFFFFF;
constexpr u64 INVALID_U64 = 0xFFFFFFFFFFFFFFFF;
constexpr s32 INVALID_INT = 0xFFFFFFFF;
constexpr u32 HCCL_INVALID_PORT = 65536;  // HCCL 默认无效端口号
// 系统常用参数
constexpr u32 MAX_MODULE_DEVICE_NUM = 32; // 单server双模组时支持最大的设备数量
constexpr s32 HOST_DEVICE_ID = -1;
constexpr char HCCL_WORLD_GROUP[] = "hccl_world_group";
constexpr u32 FACTOR_NUM_TWO = 2;
constexpr s32 DEVICE_PER_MODULE = 8;         // 单module支持最大device数量

constexpr u64 MIN_PIPLINE_SLICE_NUM = 2; // 流水并行算法最小切分次数
constexpr u32 HCCL_INTER_SERVER_RING_ALGO_MAX_SUPPORT_SERVER_NUM = 8; // server 间 ring 算法支持的最大server数: 8
constexpr s64 HCCL_SMALL_COUNT_GRAPH_64_KB = 64 * 1024;  // hccl图模式小数据量标准，暂定64KB
constexpr u32 MAX_BLOCK_DIM = 48;
constexpr int HCCL_BASE_DECIMAL = 10; // 10进制字符串转换
constexpr u32 RANK_TABLE_MAX_LEN = 4096 - 1; // PATH_MAX=4096

constexpr int HCCL_DEVICE_MINNUM = 1;
constexpr int HCCL_DEVICE_NUM_ONE = 1;
constexpr int HCCL_DEVICE_NUM_TWO = 2; // 平均device num小于等于此数值时，无法通过HCCS链路类型接口判定当前硬件环境
constexpr int HCCL_DEVICE_NUM_FOUR = 4; // 平均device num等于此数值时，需校验server内device选取合法性
constexpr int HCCL_DEVICE_NUM_EIGHT = 8;
constexpr s64 HCCL_SUB_STREAM_NUM_ZERO = 0;  // subStream 数量为0
constexpr s64 HCCL_SUB_STREAM_NUM_4P_MESH = 2;  // subStream 数量为2
constexpr s64 HCCL_SUB_STREAM_NUM_8P_RING = 3;  // subStream 数量为3

constexpr u32 HCCL_ALGO_LEVEL_1 = 1;        // HCCL 算法层级1
constexpr s64 HCCL_ALIGN_SIZE = 4096;  // hccl  对齐方式， 按4KB来对齐
constexpr s64 HCCL_MID_COUNT_16_MB = 16 * 1024 * 1024;  // 910B aiv+rdma 中数据量支持上限
constexpr s64 HCCL_MID_COUNT_32_MB = 32 * 1024 * 1024;
constexpr u32 HCCL_MEMSIZE_HD_FACTOR = 4;
constexpr u64 CCL_COMM_INBUFFER_UNALIGNED_RESERVE_SIZE = (1 * 1024 * 1024); // 1 * 1024 * 1024, 即1M
constexpr s64 HCCL_WORKSPACE_MEM_32_KB = 32768;  // hccl内存大小，暂定32KB

const std::map<HcclCMDType, std::string> HCOM_CMD_TYPE_STR_MAP{
    {HcclCMDType::HCCL_CMD_INVALID, "invalid"},
    {HcclCMDType::HCCL_CMD_BROADCAST, "broadcast"},
    {HcclCMDType::HCCL_CMD_ALLREDUCE, "allreduce"},
    {HcclCMDType::HCCL_CMD_REDUCE, "reduce"},
    {HcclCMDType::HCCL_CMD_SEND, "send"},
    {HcclCMDType::HCCL_CMD_RECEIVE, "receive"},
    {HcclCMDType::HCCL_CMD_ALLGATHER, "allgather"},
    {HcclCMDType::HCCL_CMD_ALLGATHER_V, "allgather_v"},
    {HcclCMDType::HCCL_CMD_REDUCE_SCATTER, "reduce_scatter"},
    {HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, "reduce_scatter_v"},
    {HcclCMDType::HCCL_CMD_ALLTOALLV, "alltoallv"},
    {HcclCMDType::HCCL_CMD_ALLTOALLVC, "alltoallvc"},
    {HcclCMDType::HCCL_CMD_ALLTOALL, "alltoall"},
    {HcclCMDType::HCCL_CMD_GATHER, "gather"},
    {HcclCMDType::HCCL_CMD_SCATTER, "scatter"},
    {HcclCMDType::HCCL_CMD_BATCH_SEND_RECV, "batch_send_recv"},
    {HcclCMDType::HCCL_CMD_BATCH_WRITE, "batch_write"},
    {HcclCMDType::HCCL_CMD_ALL, "all"},
    {HcclCMDType::HCCL_CMD_MAX, "max"}
};

/* 公共模块函数返回值定义,跟业务层同步  */
const std::map<HcclDataType, std::string> HCOM_DATA_TYPE_STR_MAP{
    {HcclDataType::HCCL_DATA_TYPE_INT8, "int8"},
    {HcclDataType::HCCL_DATA_TYPE_INT16, "int16"},
    {HcclDataType::HCCL_DATA_TYPE_INT32, "int32"},
    {HcclDataType::HCCL_DATA_TYPE_INT64, "int64"},
    {HcclDataType::HCCL_DATA_TYPE_UINT64, "uint64"},
    {HcclDataType::HCCL_DATA_TYPE_FP16, "float16"},
    {HcclDataType::HCCL_DATA_TYPE_FP32, "float32"},
    {HcclDataType::HCCL_DATA_TYPE_UINT8, "uint8"},
    {HcclDataType::HCCL_DATA_TYPE_UINT16, "uint16"},
    {HcclDataType::HCCL_DATA_TYPE_UINT32, "uint32"},
    {HcclDataType::HCCL_DATA_TYPE_FP64, "float64"},
    {HcclDataType::HCCL_DATA_TYPE_BFP16, "bfloat16"},
    {HcclDataType::HCCL_DATA_TYPE_INT128, "int128"},
    {HcclDataType::HCCL_DATA_TYPE_RESERVED, "reserved"}
};

// HCCL通信算法类型
enum class HcclAlgoType {
    HCCL_ALGO_TYPE_DEFAULT = 0, // 默认算法，配置为此时，使用HCCL内藏算法选择逻辑
    HCCL_ALGO_TYPE_RING,
    HCCL_ALGO_TYPE_PIPELINE,
    HCCL_ALGO_TYPE_FULLMESH,
    HCCL_ALGO_TYPE_HDR,
    HCCL_ALGO_TYPE_PAIRWISE,
    HCCL_ALGO_TYPE_NHR,
    HCCL_ALGO_TYPE_NHR_V1,
    HCCL_ALGO_TYPE_NB,
    HCCL_ALGO_TYPE_NULL,
    HCCL_ALGO_TYPE_NA,
    HCCL_ALGO_TYPE_AHC,
    HCCL_ALGO_TYPE_AHC_BROKE
};

// 对内拓扑算法枚举
enum class AlgTypeLevel0 {
    ALG_LEVEL0_WHOLE_RING = 0,  // 单层拓扑, 所有level均为Whole ring时，组成一个大环
    ALG_LEVEL0_8P_RING,         // 拓扑组合0层, Ring 节点内4个固定stream
    ALG_LEVEL0_4P_MESH,         // 拓扑组合0层, Mesh 节点内3个固定stream
    ALG_LEVEL0_2P_MESH,         // 拓扑组合0层, Mesh
    ALG_LEVEL0_1P_MESH,         // 拓扑组合0层, Mesh
    ALG_LEVEL0_4P_RING,         // 拓扑组合0层, Ring
    ALG_LEVEL0_NP_SINGLE_RING,  // 拓扑组合0层, Ring
    ALG_LEVEL0_NP_DOUBLE_RING,  // 拓扑组合0层, double Ring
    ALG_LEVEL0_NP_MESH,         // 拓扑组合0层, 服务器内3~8p rank组成MESH
    ALG_LEVEL0_NP_HD,              // 拓扑组合0层, HD
    ALG_LEVEL0_NP_STAR,
    ALG_LEVEL0_PAIRWISE,
    ALG_LEVEL0_RESERVED
};

enum class AlgTypeLevel1 {
    ALG_LEVEL1_WHOLE_RING = 0,  // 单层拓扑, 所有level均为Whole ring时，组成一个大环
    ALG_LEVEL1_HD,              // 拓扑组合1层, HDR
    ALG_LEVEL1_RING,            // 拓扑组合1层, Ring
    ALG_LEVEL1_PIPELINE,        // 拓扑组合1层, Pipeline
    ALG_LEVEL1_STAR,
    ALG_LEVEL1_NHR,             // 拓扑组合1层，NHR
    ALG_LEVEL1_NHR_V1,          // 拓扑组合1层，NHR_V1
    ALG_LEVEL1_NB,              // 拓扑组合1层，NB
    ALG_LEVEL1_AHC,             // 拓扑组合1层，AHC
    ALG_LEVEL1_AHC_BROKE,       // 拓扑组合1层，AHC_BROKE
    ALG_LEVEL1_RESERVED
};

enum class AlgTypeLevel2 {
    ALG_LEVEL2_WHOLE_RING = 0,  // 单层拓扑, 所有leve2均为Whole ring时，组成一个大环
    ALG_LEVEL2_HD,              // 拓扑组合2层, HDR
    ALG_LEVEL2_RING,            // 拓扑组合2层, Ring
    ALG_LEVEL2_NHR,             // 拓扑组合2层, NHR
    ALG_LEVEL2_NB,              // 拓扑组合2层, NB
    ALG_LEVEL2_RESERVED
};

using AlgType = struct TagAlgType {
    AlgTypeLevel0 algoLevel0;
    AlgTypeLevel1 algoLevel1;
    AlgTypeLevel2 algoLevel2;
    TagAlgType() : algoLevel0(AlgTypeLevel0::ALG_LEVEL0_WHOLE_RING), algoLevel1(AlgTypeLevel1::ALG_LEVEL1_WHOLE_RING),
        algoLevel2(AlgTypeLevel2::ALG_LEVEL2_WHOLE_RING)
    {
    }

    explicit TagAlgType(AlgTypeLevel0 algoLevel0) : algoLevel0(algoLevel0), algoLevel1(AlgTypeLevel1::ALG_LEVEL1_WHOLE_RING),
        algoLevel2(AlgTypeLevel2::ALG_LEVEL2_WHOLE_RING)
    {
    }
    TagAlgType(AlgTypeLevel0 algoLevel0, AlgTypeLevel1 algoLevel1) : algoLevel0(algoLevel0),
        algoLevel1(algoLevel1), algoLevel2(AlgTypeLevel2::ALG_LEVEL2_WHOLE_RING)
    {
    }
 
    TagAlgType(AlgTypeLevel0 algoLevel0, AlgTypeLevel2 algoLevel2) : algoLevel0(algoLevel0),
        algoLevel1(AlgTypeLevel1::ALG_LEVEL1_WHOLE_RING), algoLevel2(algoLevel2)
    {
    }
 
    TagAlgType(AlgTypeLevel0 algoLevel0, AlgTypeLevel1 algoLevel1, AlgTypeLevel2 algoLevel2) : algoLevel0(algoLevel0),
        algoLevel1(algoLevel1), algoLevel2(algoLevel2)
    {
    }
    explicit TagAlgType(AlgTypeLevel1 algoLevel1) : algoLevel0(AlgTypeLevel0::ALG_LEVEL0_WHOLE_RING), algoLevel1(algoLevel1),
        algoLevel2(AlgTypeLevel2::ALG_LEVEL2_WHOLE_RING)
    {
    }
 
    TagAlgType(AlgTypeLevel1 algoLevel1, AlgTypeLevel2 algoLevel2) : algoLevel0(AlgTypeLevel0::ALG_LEVEL0_WHOLE_RING),
        algoLevel1(algoLevel1), algoLevel2(algoLevel2)
    {
    }
 
    explicit TagAlgType(AlgTypeLevel2 algoLevel2) : algoLevel0(AlgTypeLevel0::ALG_LEVEL0_WHOLE_RING),
        algoLevel1(AlgTypeLevel1::ALG_LEVEL1_WHOLE_RING), algoLevel2(algoLevel2)
    {
    }
 
    static TagAlgType Reserved() {
        return TagAlgType(AlgTypeLevel0::ALG_LEVEL0_RESERVED, AlgTypeLevel1::ALG_LEVEL1_RESERVED, AlgTypeLevel2::ALG_LEVEL2_RESERVED);
    }
 
    TagAlgType(const TagAlgType &that) : algoLevel0(that.algoLevel0), algoLevel1(that.algoLevel1), algoLevel2(that.algoLevel2)
    {
    }
 
    TagAlgType &operator=(const TagAlgType &that)
    {
        if (&that != this) {
            algoLevel0 = that.algoLevel0;
            algoLevel1 = that.algoLevel1;
            algoLevel2 = that.algoLevel2;
        }
        return *this;
    }
};

const std::map<AlgTypeLevel0, std::string> HCCL_ALGO_LEVEL0_NAME_MAP = {
    {AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING, "ring"},
    {AlgTypeLevel0::ALG_LEVEL0_WHOLE_RING, "ring"},
    {AlgTypeLevel0::ALG_LEVEL0_8P_RING, "ring"},
    {AlgTypeLevel0::ALG_LEVEL0_4P_MESH, "fullmesh"},
    {AlgTypeLevel0::ALG_LEVEL0_2P_MESH, "fullmesh"},
    {AlgTypeLevel0::ALG_LEVEL0_1P_MESH, "fullmesh"},
    {AlgTypeLevel0::ALG_LEVEL0_4P_RING, "ring"},
    {AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING, "ring"},
    {AlgTypeLevel0::ALG_LEVEL0_NP_MESH, "fullmesh"},
    {AlgTypeLevel0::ALG_LEVEL0_NP_HD, "HD"},
    {AlgTypeLevel0::ALG_LEVEL0_NP_STAR, "star"},
    {AlgTypeLevel0::ALG_LEVEL0_RESERVED, "null"},
};

const std::map<AlgTypeLevel1, std::string> HCCL_ALGO_LEVEL1_NAME_MAP = {
    {AlgTypeLevel1::ALG_LEVEL1_WHOLE_RING, "ring"},
    {AlgTypeLevel1::ALG_LEVEL1_HD, "H-D"},
    {AlgTypeLevel1::ALG_LEVEL1_RING, "ring"},
    {AlgTypeLevel1::ALG_LEVEL1_PIPELINE, "pipeline"},
    {AlgTypeLevel1::ALG_LEVEL1_NHR, "NHR"},
    {AlgTypeLevel1::ALG_LEVEL1_NHR_V1, "NHR_V1"},
    {AlgTypeLevel1::ALG_LEVEL1_AHC, "AHC"},
    {AlgTypeLevel1::ALG_LEVEL1_AHC_BROKE, "AHC_BROKE"},
    {AlgTypeLevel1::ALG_LEVEL1_NB, "NB"},
    {AlgTypeLevel1::ALG_LEVEL1_RESERVED, "null"},
};

const std::map<AlgTypeLevel2, std::string> HCCL_ALGO_LEVEL2_NAME_MAP = {
    {AlgTypeLevel2::ALG_LEVEL2_WHOLE_RING, "ring"},
    {AlgTypeLevel2::ALG_LEVEL2_HD, "H-D"},
    {AlgTypeLevel2::ALG_LEVEL2_RING, "ring"},
    {AlgTypeLevel2::ALG_LEVEL2_NHR, "NHR"},
    {AlgTypeLevel2::ALG_LEVEL2_NB, "NB"},
    {AlgTypeLevel2::ALG_LEVEL2_RESERVED, "null"},
};

typedef enum {
    DETERMINISTIC_DISABLE = 0,          // 不支持确定性
    DETERMINISTIC_ENABLE,               // 支持确定性，不支持规约保序
    DETERMINISTIC_STRICT                // 支持确定性以及规约保序
} DeterministicEnableLevel;

// 全局工作空间类型
enum class GlobalWorkSpaceType {
    OVERFLOW_DETECT_MODE = 0,
};

enum class HcclTopoLevel {
    HCCL_TOPO_L0 = 0,
    HCCL_TOPO_L1,
    HCCL_TOPO_MAX,
};

constexpr u32 SIZE_TABLE[HCCL_DATA_TYPE_RESERVED] = {sizeof(s8), sizeof(s16), sizeof(s32),
    2, sizeof(float), sizeof(s64), sizeof(u64), sizeof(u8), sizeof(u16), sizeof(u32), 8, 2, 16};

#endif /* HCCE_COMMON_H */