/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_OP_TILING_H
#define INC_OP_TILING_H
#define FMK_FUNC_HOST_VISIBILITY_HCCL __attribute__((visibility("default")))

#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <functional>
#include <vector>
#include <map>
#include <string>
#include "legacy_log.h"
#include "exe_graph/runtime/tiling_context.h"

#define REGISTER_OP_TILING_FUNC(optype, opfunc) REGISTER_OP_TILING_FUNC_UNIQ_HELPER(optype, opfunc, __COUNTER__)

#define REGISTER_OP_TILING_FUNC_UNIQ_HELPER(optype, opfunc, counter) \
    REGISTER_OP_TILING_FUNC_UNIQ(optype, opfunc, counter)

#define REGISTER_OP_TILING_FUNC_UNIQ(optype, opfunc, counter) \
    static OpTilingInterf g_##optype##TilingInterf##counter(#optype, opfunc)

#define CHECK(cond, message, ...)          \
    if (!cond) {                           \
        HCCL_ERROR(#message, __VA_ARGS__); \
        return false;                      \
    }

#define CHECK_WARNING(cond, message, ...)    \
    if (!cond) {                             \
        HCCL_WARNING(#message, __VA_ARGS__); \
        return false;                        \
    }

#define CHECK_EQ(x, y, message, ...) CHECK((x == y), message, __VA_ARGS__)
#define CHECK_NE(x, y, message, ...) CHECK((x != y), message, __VA_ARGS__)
#define CHECK_GT(x, y, message, ...) CHECK((x > y), message, __VA_ARGS__)
#define CHECK_GE(x, y, message, ...) CHECK((x >= y), message, __VA_ARGS__)
#define CHECK_LT(x, y, message, ...) CHECK((x < y), message, __VA_ARGS__)
#define CHECK_LE(x, y, message, ...) CHECK((x <= y), message, __VA_ARGS__)

#define OP_TILING_MAKE_SHARED(exec_expr0, exec_expr1) \
    do {                                              \
        try {                                         \
            exec_expr0;                               \
        } catch (...) {                               \
            HCCL_INFO("Make shared failed");          \
            exec_expr1;                               \
        }                                             \
    } while (0)

namespace TbeReduce {
enum class TensorArgType {
    TA_NONE,
    TA_SINGLE,
    TA_LIST,
};

using ByteBuffer = std::stringstream;

struct TeOpTensor {
    std::vector<int64_t> shape;
    std::vector<int64_t> oriShape;
    std::string format;
    std::string oriFormat;
    std::string dtype;
    std::map<std::string, std::string> attrs;
};

struct TeOpTensorArg {
    TensorArgType argType;
    std::vector<TeOpTensor> tensor;
};

struct OpRunInfo {
    uint32_t numBlocks;
    std::vector<int64_t> workspaces;
    ByteBuffer tilingData;
    bool clearAtomic;
    uint32_t tilingKey;
};

using TeOpAttrArgs = std::vector<std::string>;

struct TeOpParas {
    std::vector<TeOpTensorArg> inputs;
    std::vector<TeOpTensorArg> outputs;
    TeOpAttrArgs attrs;
};

using OpTilingFunc = std::function<bool(const std::string &, const TeOpParas &, const nlohmann::json &, OpRunInfo &)>;
using OpTilingFuncPtr = bool (*)(const std::string &, const TeOpParas &, const nlohmann::json &, OpRunInfo &);
class FMK_FUNC_HOST_VISIBILITY_HCCL OpTilingInterf {
public:
    OpTilingInterf(std::string opType, OpTilingFunc func);
    ~OpTilingInterf() = default;
    static std::map<std::string, OpTilingFunc> &RegisteredOpInterf();
    static std::string OpTilingUuid;
};

template <class T> ByteBuffer &ByteBufferPut(ByteBuffer &buf, const T &value)
{
    buf.write(reinterpret_cast<const char *>(&value), sizeof(value));
    buf.flush();
    return buf;
}

template <class T> ByteBuffer &ByteBufferGet(ByteBuffer &buf, T &value)
{
    buf.read(reinterpret_cast<char *>(&value), sizeof(value));
    return buf;
}

inline size_t ByteBufferGetAll(ByteBuffer &buf, char *dest, size_t dest_len)
{
    size_t nread = 0;
    size_t rn = 0;
    do {
        rn = buf.readsome(dest + nread, dest_len - nread);
        nread += rn;
    } while (rn > 0 && dest_len > nread);

    return nread;
}

template <typename T, typename TR> bool RoundUpOverflow(T value, T multiple_of, TR &ret)
{
    if (multiple_of == 0) {
        ret = 0;
        return true;
    }
    auto remainder = value % multiple_of;
    if (remainder == 0) {
        if (!ge::IntegerChecker<TR>::Compat(value)) {
            return true;
        }
        ret = static_cast<TR>(value);
        return false;
    }
    return ge::AddOverflow(value - remainder, multiple_of, ret);
}


inline uint64_t RoundUp(const uint64_t origin_value, const uint64_t multiple_of)
{
    uint64_t ret = 0U;
    if (RoundUpOverflow(origin_value, multiple_of, ret)) {
        return 0U;
    }
    return ret;
}

template <typename T> std::string GetTilingDataString(gert::TilingContext *context)
{
    auto tiling_data = context->GetRawTilingData();
    auto data_size = tiling_data->GetDataSize();
    std::string result;
    const T *data = reinterpret_cast<const T *>(tiling_data->GetData());
    size_t len = data_size / sizeof(T);
    for (size_t i = 0; i < len; i++) {
        result += std::to_string(data[i]);
        result += " ";
    }
    return result;
}

template<typename TI, typename TO>
inline TO *PtrToPtr(TI *const ptr) {
    return reinterpret_cast<TO *>(ptr);
}

template<typename TI, typename TO>
inline const TO *PtrToPtr(const TI *const ptr) {
    return reinterpret_cast<const TO *>(ptr);
}

enum class SchPattern {
    ELETWISE = 0X10,
    BROADCAST = 0x11,
    COMMONREDUCE = 0x12,
    TUPLEREDUCE = 0x13,
    NORM = 0x14,
    CONCAT = 0x15,
    SPLIT = 0x16,
    GATHER = 0x17,
    TRANSPOSE = 0x18,
    TRANSDATA = 0x19,
    SLICE = 0x1A,
    POOLING = 0x1B,
    SORT = 0x1C,
    SPARSEAPPLY = 0x1D,
    SCATTER = 0x1E,
    RESIZE = 0x1F,
    AVGPOOLUPDATE = 0x20,
    POOLINGWITHARG = 0x21,
    POOLINGGRADWITHARG = 0x22,
    SEGMENT = 0x23,
    POOLINGGRAD = 0x24,
    GENERICVECTOR = 0x25,
    DEFAULT
};
}

#endif // INC_OP_TILING_H
