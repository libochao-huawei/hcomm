/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include <mutex>
#include <vector>
#include <iostream>
#include <fstream>
#include "mmpa_api.h"
#include "orion_adapter_rts.h"
#include "hccl_aiv_utils.h"
 
using namespace std;
using BinHandle = void *;
 
namespace Hccl {
constexpr u32 SIG_MOVE_LEFT_BITS = 20;
 
constexpr u32 AIV_BUFFER_PING_PONG_FACTOR = 2;
 
constexpr u32 MAX_BIN_FILE_SIZE = 100 * 1024 * 1024; // 最大读取100m的bin file到string中
 
constexpr s32 RESET_TAIL_SYNC_TAG = 2;
 
using AivKernelInfo = struct AivKernelInfoDef {
    const char* kernelName;
    HcclCMDType cmdType;
    DataType dataType;
    KernelArgsType argsType;
 
    AivKernelInfoDef(const char* kernelName, HcclCMDType cmdType, DataType dataType,
        KernelArgsType argsType = KernelArgsType::ARGS_TYPE_SERVER)
        : kernelName(kernelName), cmdType(cmdType), dataType(dataType), argsType(argsType)
    {
    }
};
 
static std::vector<AivKernelInfo> g_aivKernelInfoList = {
    // scatter
    {"aiv_scatter_half", HcclCMDType::HCCL_CMD_SCATTER, DataType::FP16},
    {"aiv_scatter_int16_t", HcclCMDType::HCCL_CMD_SCATTER, DataType::INT16},
    {"aiv_scatter_uint16_t", HcclCMDType::HCCL_CMD_SCATTER, DataType::UINT16},
    {"aiv_scatter_float", HcclCMDType::HCCL_CMD_SCATTER, DataType::FP32},
    {"aiv_scatter_int32_t", HcclCMDType::HCCL_CMD_SCATTER, DataType::INT32},
    {"aiv_scatter_uint32_t", HcclCMDType::HCCL_CMD_SCATTER, DataType::UINT32},
    {"aiv_scatter_int8_t", HcclCMDType::HCCL_CMD_SCATTER, DataType::INT8},
    {"aiv_scatter_uint8_t", HcclCMDType::HCCL_CMD_SCATTER, DataType::UINT8},
    {"aiv_scatter_bfloat16_t", HcclCMDType::HCCL_CMD_SCATTER, DataType::BFP16},
    {"aiv_scatter_uint64_t", HcclCMDType::HCCL_CMD_SCATTER, DataType::INT64},
    {"aiv_scatter_int64_t", HcclCMDType::HCCL_CMD_SCATTER, DataType::UINT64},

    {"aiv_all_gather_half", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::FP16},
    {"aiv_all_gather_int16_t", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::INT16},
    {"aiv_all_gather_uint16_t", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::UINT16},
    {"aiv_all_gather_float", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::FP32},
    {"aiv_all_gather_int32_t", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::INT32},
    {"aiv_all_gather_uint32_t", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::UINT32},
    {"aiv_all_gather_int8_t", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::INT8},
    {"aiv_all_gather_uint8_t", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::UINT8},
    {"aiv_all_gather_bfloat16_t", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::BFP16},
    {"aiv_all_gather_uint64_t", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::INT64},
    {"aiv_all_gather_int64_t", HcclCMDType::HCCL_CMD_ALLGATHER, DataType::UINT64},
    //allreduce
    {"aiv_allreduce_half", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::FP16},
    {"aiv_allreduce_int16_t", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::INT16},
    {"aiv_allreduce_float", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::FP32},
    {"aiv_allreduce_int32_t", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::INT32},
    {"aiv_allreduce_int8_t", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::INT8},
    {"aiv_allreduce_bfloat16_t", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::BFP16},
    //broadcast
    {"aiv_broadcast_half", HcclCMDType::HCCL_CMD_BROADCAST, DataType::FP16},
    {"aiv_broadcast_int16_t", HcclCMDType::HCCL_CMD_BROADCAST, DataType::INT16},
    {"aiv_broadcast_uint16_t", HcclCMDType::HCCL_CMD_BROADCAST, DataType::UINT16},
    {"aiv_broadcast_float", HcclCMDType::HCCL_CMD_BROADCAST, DataType::FP32},
    {"aiv_broadcast_int32_t", HcclCMDType::HCCL_CMD_BROADCAST, DataType::INT32},
    {"aiv_broadcast_uint32_t", HcclCMDType::HCCL_CMD_BROADCAST, DataType::UINT32},
    {"aiv_broadcast_int8_t", HcclCMDType::HCCL_CMD_BROADCAST, DataType::INT8},
    {"aiv_broadcast_uint8_t", HcclCMDType::HCCL_CMD_BROADCAST, DataType::UINT8},
    {"aiv_broadcast_bfloat16_t", HcclCMDType::HCCL_CMD_BROADCAST, DataType::BFP16},
    {"aiv_broadcast_uint64_t", HcclCMDType::HCCL_CMD_BROADCAST, DataType::INT64},
    {"aiv_broadcast_int64_t", HcclCMDType::HCCL_CMD_BROADCAST, DataType::UINT64},
    // allreduce two shot
    {"aiv_allreduce_mesh1d_twoshot_half", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::FP16, KernelArgsType::ARGS_TYPE_TWO_SHOT},
    {"aiv_allreduce_mesh1d_twoshot_int16_t", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::INT16,KernelArgsType::ARGS_TYPE_TWO_SHOT},
    {"aiv_allreduce_mesh1d_twoshot_float", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::FP32,KernelArgsType::ARGS_TYPE_TWO_SHOT},
    {"aiv_allreduce_mesh1d_twoshot_int32_t", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::INT32,KernelArgsType::ARGS_TYPE_TWO_SHOT},
    {"aiv_allreduce_mesh1d_twoshot_int8_t", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::INT8,KernelArgsType::ARGS_TYPE_TWO_SHOT},
    {"aiv_allreduce_mesh1d_twoshot_bfloat16_t", HcclCMDType::HCCL_CMD_ALLREDUCE, DataType::BFP16,KernelArgsType::ARGS_TYPE_TWO_SHOT},
    // alltoall
    {"aiv_alltoall_half", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::FP16},
    {"aiv_alltoall_int16_t", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::INT16},
    {"aiv_alltoall_uint16_t", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::UINT16},
    {"aiv_alltoall_float", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::FP32},
    {"aiv_alltoall_int32_t", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::INT32},
    {"aiv_alltoall_uint32_t", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::UINT32},
    {"aiv_alltoall_int8_t", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::INT8},
    {"aiv_alltoall_uint8_t", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::UINT8},
    {"aiv_alltoall_bfloat16_t", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::BFP16},
    {"aiv_alltoall_uint64_t", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::INT64},
    {"aiv_alltoall_int64_t", HcclCMDType::HCCL_CMD_ALLTOALL, DataType::UINT64},
    // alltoallv
    {"aiv_alltoallv_half", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::FP16},
    {"aiv_alltoallv_int16_t", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::INT16},
    {"aiv_alltoallv_uint16_t", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::UINT16},
    {"aiv_alltoallv_float", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::FP32},
    {"aiv_alltoallv_int32_t", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::INT32},
    {"aiv_alltoallv_uint32_t", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::UINT32},
    {"aiv_alltoallv_int8_t", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::INT8},
    {"aiv_alltoallv_uint8_t", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::UINT8},
    {"aiv_alltoallv_bfloat16_t", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::BFP16},
    {"aiv_alltoallv_uint64_t", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::INT64},
    {"aiv_alltoallv_int64_t", HcclCMDType::HCCL_CMD_ALLTOALLV, DataType::UINT64},
    // reduce
    {"aiv_reduce_half", HcclCMDType::HCCL_CMD_REDUCE, DataType::FP16},
    {"aiv_reduce_int16_t", HcclCMDType::HCCL_CMD_REDUCE, DataType::INT16},
    {"aiv_reduce_float", HcclCMDType::HCCL_CMD_REDUCE, DataType::FP32},
    {"aiv_reduce_int32_t", HcclCMDType::HCCL_CMD_REDUCE, DataType::INT32},
    {"aiv_reduce_int8_t", HcclCMDType::HCCL_CMD_REDUCE, DataType::INT8},
    {"aiv_reduce_bfloat16_t", HcclCMDType::HCCL_CMD_REDUCE, DataType::BFP16},
    //reducescatter
    {"aiv_reduce_scatter_half", HcclCMDType::HCCL_CMD_REDUCE_SCATTER, DataType::FP16},
    {"aiv_reduce_scatter_int16_t", HcclCMDType::HCCL_CMD_REDUCE_SCATTER, DataType::INT16},
    {"aiv_reduce_scatter_float", HcclCMDType::HCCL_CMD_REDUCE_SCATTER, DataType::FP32},
    {"aiv_reduce_scatter_int32_t", HcclCMDType::HCCL_CMD_REDUCE_SCATTER, DataType::INT32},
    {"aiv_reduce_scatter_int8_t", HcclCMDType::HCCL_CMD_REDUCE_SCATTER, DataType::INT8},
    {"aiv_reduce_scatter_bfloat16_t", HcclCMDType::HCCL_CMD_REDUCE_SCATTER, DataType::BFP16},
};
using AivExtraKernelArgs = struct AivExtraKernelArgsDef {
    const void* buffersIn; // 注册的CCLIN地址，所有卡可访问
    u64 input;
    u64 output;
    u32 rank;
    u32 rankSize;
    u64 xRankSize;
    u64 yRankSize;
    u64 zRankSize;
    u64 len;
    u32 dataType;
    u32 reduceOp;
    u32 root;
    u32 tag; // 第几次调用，定时重置成1
    u64 inputSliceStride;
    u64 outputSliceStride;
    u64 repeatNum;
    u64 inputRepeatStride;
    u64 outputRepeatStride;
    bool isOpBase;
    const void* headCountMem;
    const void* tailCountMem;
    const void* addOneMem;
    u32 counterMemSize;
    bool isEnableCounter;
    ExtraArgsA2A extraArgs; 
 
    AivExtraKernelArgsDef(const void* buffIn, u64 input, u64 output, u32 rank,
        u32 rankSize, u64 xRankSize, u64 yRankSize, u64 zRankSize,
        u64 len, u32 dataType, u32 reduceOp, u32 root, u32 tag, 
        u64 inputSliceStride, u64 outputSliceStride, u64 repeatNum, u64 inputRepeatStride, u64 outputRepeatStride,
        bool isOpBase = true,
        const void* headCountMem = nullptr, const void* tailCountMem = nullptr, const void* addOneMem = nullptr,
        u32 counterMemSize = 0, const ExtraArgsA2A* extraArgsPtr = nullptr)
        : buffersIn(buffIn),input(input), output(output), rank(rank), rankSize(rankSize), xRankSize(xRankSize), yRankSize(yRankSize), zRankSize(zRankSize),
        len(len) ,dataType(dataType),
        reduceOp(reduceOp), root(root), tag(tag), 
        inputSliceStride(inputSliceStride), outputSliceStride(outputSliceStride), repeatNum(repeatNum), inputRepeatStride(inputRepeatStride), outputRepeatStride(outputRepeatStride),
        isOpBase(isOpBase), 
        headCountMem(headCountMem), tailCountMem(tailCountMem), addOneMem(addOneMem),
        counterMemSize(counterMemSize)
    {
        if (extraArgsPtr != nullptr) {
            extraArgs = *extraArgsPtr;
        }
    }
};
 
HcclResult GetAivOpBinaryPath(std::string &binaryPath)
{
    // 获取二进制文件路径
    std::string libPath;
    if (const auto* env = getenv("LD_LIBRARY_PATH")) {
        libPath = env;
    } else {
        HCCL_ERROR("[AIV][GetAivOpBinaryPath]ENV:LD_LIBRARY_PATH is not set");
        return HCCL_E_PARA;
    }
 
    size_t mid = libPath.find("fwkacllib/lib64");
    if (mid == libPath.npos) {
        HCCL_WARNING("[AIV][GetAivOpBinaryPath]ENV:LD_LIBRARY_PATH lack fwkacllib/lib64");
 
        mmDlInfo info;
        mmDladdr(reinterpret_cast<void *>(RegisterKernel), &info);
 
        CHK_PRT_RET(info.dli_fname == nullptr, HCCL_ERROR("[AIV][GetAivOpBinaryPath]get path of libhccl_alg.so failed"),
            HCCL_E_UNAVAIL);
 
        char resolvedPath[PATH_MAX];
        if (realpath(info.dli_fname, resolvedPath) == nullptr) {
            HCCL_ERROR("[AIV][GetAivOpBinaryPath]path %s is not a valid real path", info.dli_fname);
            return HCCL_E_INTERNAL;
        }
        binaryPath = resolvedPath;
        if (binaryPath.find("/libhccl_v2.so") != binaryPath.npos) {
            binaryPath.erase(binaryPath.find("/libhccl_v2.so"));
        } else {
            HCCL_ERROR("[AIV][GetAivOpBinaryPath]get binary path failed");
            return HCCL_E_PARA;
        }
        HCCL_DEBUG("[AIV][GetAivOpBinaryPath]op binary file path[%s]", binaryPath.c_str());
    } else {
        u32 diff;
        if (libPath.find(":", mid) == libPath.npos) {
            diff = libPath.length() - libPath.rfind(":", mid);
        } else {
            diff = libPath.find(":", mid) - libPath.rfind(":", mid);
        }
        binaryPath = libPath.substr(libPath.rfind(":", mid) + 1, diff - 1);
    }
 
    // 判断应该加载的文件
     binaryPath += "/hccl_aiv_op_910_95.o";
    return HCCL_SUCCESS;
}
 
HcclResult ReadBinFile(const string& fileName, string& buffer)
{
    char realFile[PATH_MAX] = { 0 };
    if (realpath(fileName.c_str(), realFile) == nullptr)
    {
        HCCL_INFO("[AIV][ReadBinFile] Binfile path %s is not a valid real path.", realFile);
        return HCCL_E_NOT_FOUND;
    }
    std::ifstream filestr;
    filestr.open(realFile, std::ios::binary);
    if (!filestr) {
        HCCL_ERROR("[AIV][ReadBinFile]open file [%s] failed!", fileName.c_str());
        return HCCL_E_OPEN_FILE_FAILURE;
    }
 
    filestr.seekg(0, std::ios::end);
    std::streampos fileSize = filestr.tellg();
    filestr.seekg(0, std::ios::beg);
 
    if (fileSize == 0 || fileSize >= MAX_BIN_FILE_SIZE) {
        HCCL_ERROR("[AIV][ReadBinFile] file [%s] size is invalid, is [%d]!", fileName.c_str(), fileSize);
        filestr.close();
        return HCCL_E_OPEN_FILE_FAILURE;
    }
    buffer.resize(fileSize);
    filestr.read(&buffer[0], fileSize);
 
    filestr.close();
    return HCCL_SUCCESS;
}
 
s8* GetStubFunc(HcclCMDType cmdType, DataType dataType, KernelArgsType argsType = KernelArgsType::ARGS_TYPE_SERVER)
{
    return reinterpret_cast<s8*>(
        (((static_cast<s64>(cmdType) << SIG_MOVE_LEFT_BITS) + static_cast<s64>(dataType)) << SIG_MOVE_LEFT_BITS) +
        static_cast<s64>(argsType));
}
 
HcclResult RegisterBinaryKernel(const char* funcName, const string &binFile, s8* stubFunc)
{
    rtDevBinary_t binary;
    void* binHandle = nullptr;
 
    binary.data = binFile.c_str();
    binary.length = binFile.size();
    binary.magic = RT_DEV_BINARY_MAGIC_ELF_AIVEC; // AIV算子
    binary.version = 0;
 
    binHandle = HrtDevBinaryRegister(&binary);
 
    HrtFunctionRegister(binHandle, stubFunc, funcName, funcName, 0);
    return HCCL_SUCCESS;
}
 
// Kernel注册入口，全局只需要初始化一次
HcclResult RegisterKernel()
{
    static bool init = false;
    static mutex mut;
    lock_guard<mutex> guard(mut);
    if (init) {
        return HCCL_SUCCESS;
    }
 
    HcclResult ret;
    string binFilePath;
    ret = GetAivOpBinaryPath(binFilePath);
    HCCL_INFO("[RegisterKernel] binFilePath: %s", binFilePath.c_str());
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[AIV][RegisterKernel] get aiv op binary path failed"), HCCL_E_RUNTIME);
 
    static string buffer;
    ret = ReadBinFile(binFilePath, buffer);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[AIV][RegisterKernel] read aiv kernel bin file failed"),
        HCCL_E_RUNTIME);
 
    for (auto &aivKernelInfo: g_aivKernelInfoList) {
        ret = RegisterBinaryKernel(aivKernelInfo.kernelName, buffer,
            GetStubFunc(aivKernelInfo.cmdType, aivKernelInfo.dataType, aivKernelInfo.argsType));

        CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[AIV][RegisterKernel] register binary kernel for kernelName[%s] "
            "cmdType[%d] dataType[%d] argsType[%d] failed", aivKernelInfo.kernelName, aivKernelInfo.cmdType,
            aivKernelInfo.dataType, aivKernelInfo.argsType), HCCL_E_RUNTIME);
    }
 
    init = true;
 
    return HCCL_SUCCESS;
}
 
// KernelLaunch内部接口
HcclResult ExecuteKernelLaunchInner(const AivOpArgs &opArgs, void* args, u32 argsSize)
{
    HCCL_INFO("[AIV][ExecuteKernelLaunch] sendbuff [%llu] recvbuff [%llu] rank [%u] rankSize [%u] count [%llu] "
        "dataType [%d] reduceOp [%d] root [%u] tag [%u] isOpBase [%d]"
        "extraArgsPtr [%p] argsSize [%u]", opArgs.input,
        opArgs.output, opArgs.rank, opArgs.rankSize, opArgs.count,
        opArgs.dataType, opArgs.op, opArgs.root,
        opArgs.aivTag, opArgs.isOpBase, args, argsSize);
 
    rtTaskCfgInfo_t taskCfgInfo = { 0, 0, 1 };
    rtArgsEx_t argsEx {args, nullptr, argsSize, 0, 0, 0, 0, 0 };

    HrtKernelLaunchWithFlagV2(GetStubFunc(opArgs.cmdType, opArgs.dataType, opArgs.argsType),
        opArgs.blockDim, &argsEx, nullptr, opArgs.stream, 0, &taskCfgInfo);
    return HCCL_SUCCESS;
}
 
// Kernel单次调用Launch外部接口
HcclResult ExecuteKernelLaunch(const AivOpArgs &opArgs)
{
    AivExtraKernelArgs aivExtraKernelArgs {
        opArgs.buffersIn, opArgs.input, opArgs.output,
        opArgs.rank, opArgs.rankSize, opArgs.xRankSize, opArgs.yRankSize, opArgs.zRankSize, opArgs.count, opArgs.dataType, opArgs.op, opArgs.root, opArgs.aivTag,
        opArgs.inputSliceStride, opArgs.outputSliceStride, opArgs.repeatNum, opArgs.inputRepeatStride, opArgs.outputRepeatStride,
        opArgs.isOpBase, 
        reinterpret_cast<void*>(opArgs.counter.headCountMem),
        reinterpret_cast<void*>(opArgs.counter.tailCountMem), reinterpret_cast<void*>(opArgs.counter.addOneMem),
        opArgs.counter.memSize, &opArgs.extraArgs
    };
    CHK_RET(ExecuteKernelLaunchInner(opArgs, &aivExtraKernelArgs, sizeof(aivExtraKernelArgs)));
 
    return HCCL_SUCCESS;
}
 
}   // ~~ namespace hccl