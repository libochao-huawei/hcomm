/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef OPS_BUILT_IN_OP_TILING_AUTO_TILING_CONTEXT_H
#define OPS_BUILT_IN_OP_TILING_AUTO_TILING_CONTEXT_H

#include <iterator>
#include "exe_graph/runtime/tiling_context.h"
#include "vector_op_info.h"
#include "op_tiling.h"
#include "vector_tiling_rt2.h"

namespace TbeReduce {
const gert::Shape g_vec_1_shape = {1};

const int32_t AXIS_CHWN_DIM_C = 0;
const int32_t AXIS_NCHW_DIM_C = 1;
const int32_t AXIS_HWCN_DIM_C = 2;
const int32_t AXIS_NHWC_DIM_C = 3;
const int32_t NCDHW_DIM_C = 1;
const int32_t DHWCN_DIM_C = 3;
const int32_t NDHWC_DIM_C = 4;
const int32_t DHWNC_DIM_C = 4;

const std::map<ge::Format, const int32_t> CDIM_INDEX_OF_FORMAT {
    {ge::FORMAT_NCHW, AXIS_NCHW_DIM_C},
    {ge::FORMAT_HWCN, AXIS_HWCN_DIM_C},
    {ge::FORMAT_NHWC, AXIS_NHWC_DIM_C},
    {ge::FORMAT_CHWN, AXIS_CHWN_DIM_C},
    {ge::FORMAT_NDHWC, NDHWC_DIM_C},
    {ge::FORMAT_NCDHW, NCDHW_DIM_C},
    {ge::FORMAT_DHWCN, DHWCN_DIM_C},
    {ge::FORMAT_DHWNC, DHWNC_DIM_C}
};

class OpShape {
public:
    OpShape() = default;
    explicit OpShape(const gert::Shape *_rt_shape) : rt_shape(_rt_shape){};
    ~OpShape() = default;
    inline size_t GetDimNum() const
    {
        return rt_shape->GetDimNum();
    };
    inline int64_t GetDim(size_t idx) const
    {
        return rt_shape->GetDim(idx);
    };
    inline int64_t GetShapeSize() const
    {
        return rt_shape->GetShapeSize();
    };
    inline bool Empty() const
    {
        return rt_shape == nullptr;
    };

private:
    const gert::Shape *rt_shape{ nullptr };
};

static const OpShape empty_shape;

class AutoTilingContext {
public:
    explicit AutoTilingContext(gert::TilingContext *_context) : context(_context)
    {
        tiling_data = context->GetRawTilingData();
    }
    explicit AutoTilingContext(gert::TilingContext *_context, const AutoTilingCompileInfo *_compile_info)
        : context(_context), compile_info(_compile_info)
    {
        tiling_data = context->GetRawTilingData();
    }
    ~AutoTilingContext() = default;

public:
    bool OutputGetSubFormat(size_t idx, int32_t &groups);
    bool GetInputFormat(size_t idx, ge::Format &format);
    bool GetInputOriFormat(size_t idx, ge::Format &format);
    bool GetOutputFormat(size_t idx, ge::Format &format);
    bool GetInputDataType(size_t idx, ge::DataType &dtype)
    {
        auto input_desc = context->GetInputDesc(idx);
        if (input_desc == nullptr) {
            return false;
        }
        dtype = input_desc->GetDataType();
        return true;
    }
    bool GetInputDataType(const OpInfoImpl *op_info, ge::DataType &dtype)
    {
        if (op_info && op_info->GetInType()) {
            dtype = *op_info->GetInType();
            return true;
        }
        return GetInputDataType(static_cast<size_t>(0), dtype);
    }
    bool GetOutputDataType(size_t idx, ge::DataType &dtype)
    {
        auto output_desc = context->GetOutputDesc(idx);
        if (output_desc == nullptr) {
            return false;
        }
        dtype = output_desc->GetDataType();
        return true;
    }
    size_t GetInputNums()
    {
        return context->GetComputeNodeInputNum();
    }
    size_t GetInputNums(const OpInfoImpl *op_info)
    {
        if (op_info != nullptr && op_info->GetInputNum() > 0) {
            HCCL_DEBUG("Get custom input shape num success");
            return op_info->GetInputNum();
        }
        return GetInputNums();
    }

    size_t GetOutputNums()
    {
        auto compute_node_info = context->GetComputeNodeInfo();
        if (compute_node_info == nullptr) {
            return 0;
        }
        return compute_node_info->GetOutputsNum();
    }

    inline const gert::Shape &EnsureNotScalar(const gert::Shape &in_shape) {
        if (in_shape.IsScalar()) {
            return g_vec_1_shape;
        }
        return in_shape;
    }

    OpShape GetInputShape(size_t idx)
    {
        auto input_shape = context->GetInputShape(idx);
        if (input_shape == nullptr) {
            return empty_shape;
        }
        const gert::Shape &shape = EnsureNotScalar(input_shape->GetStorageShape());
        OpShape opShape(&shape);
        return opShape;
    }
    OpShape GetOriginInputShape(size_t idx);
    OpShape GetOutputShape(size_t idx)
    {
        const gert::Shape &shape = EnsureNotScalar(context->GetOutputShape(idx)->GetStorageShape());
        OpShape opShape(&shape);
        return opShape;
    }
    OpShape GetOriginOutputShape(size_t idx);
    OpShape GetOutputShape(const OpInfoImpl *op_info, size_t idx);
    const char *GetOpType()
    {
        return context->GetNodeType();
    }
    const char *GetOpName()
    {
        return context->GetNodeName();
    }
    const AutoTilingCompileInfo *GetCompileInfo()
    {
        if (compile_info != nullptr) {
            return compile_info;
        }
        return reinterpret_cast<const AutoTilingCompileInfo *>(context->GetCompileInfo());
    }
    bool SetNumBlocks(uint32_t num_blocks)
    {
        if (context->SetBlockDim(num_blocks) == ge::GRAPH_FAILED) {
            return false;
        }
        return true;
    }
    bool SetTilingKey(uint64_t tiling_key)
    {
        if (context->SetTilingKey(tiling_key) == ge::GRAPH_FAILED) {
            return false;
        }
        return true;
    }
    bool SetNeedAtomic(bool flag);
    void SetCompileInfo(const AutoTilingCompileInfo *_compile_info)
    {
        compile_info = _compile_info;
    }
    bool GetAttr(const char *name, size_t index, bool &value);
    bool GetAttr(const char *name, size_t index, int64_t &value);
    bool GetAttr(const char *name, size_t index, std::vector<int64_t> &values);
    bool GetAttr(const char *name, size_t index, std::string &value);
    bool GetAttr(const char *name, size_t index, float &value);
    bool WriteVarAttrs(const uint64_t tiling_key)
    {
        const AutoTilingCompileInfo *autoTilingCompileInfo =
            static_cast<const AutoTilingCompileInfo *>(GetCompileInfo());
        if (autoTilingCompileInfo->var_attr_wrap.IsEmpty()) {
            return true;
        }
        return autoTilingCompileInfo->var_attr_wrap.WriteVarAttrs(tiling_key, *context);
    }

    template <typename ForwardIterator> bool AddWorkspace(ForwardIterator first, size_t n)
    {
        auto workspace_data = context->GetWorkspaceSizes(n);
        if (workspace_data == nullptr) {
            return false;
        }
        for (size_t i = 0; i < n; i++) {
            workspace_data[i] = *first;
            first++;
        }
        return true;
    }

    template <typename T> bool Append(const T &data)
    {
        if (tiling_data->Append(data) == ge::GRAPH_FAILED) {
            return false;
        }
        return true;
    }

    template <typename T> bool Append(const T *data, size_t tiling_num)
    {
        if (tiling_data->Append(data, tiling_num) == ge::GRAPH_FAILED) {
            return false;
        }
        return true;
    }

private:
    gert::TilingContext *context{ nullptr };
    gert::TilingData *tiling_data{ nullptr };
    const AutoTilingCompileInfo *compile_info{ nullptr };
};
} // namespace optiling

#endif // OPS_BUILT_IN_OP_TILING_AUTO_TILING_CONTEXT_H
