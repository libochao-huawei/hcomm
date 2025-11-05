/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef OPS_BUILT_IN_OP_TILING_VECTOR_OP_INFO_H
#define OPS_BUILT_IN_OP_TILING_VECTOR_OP_INFO_H


#include <vector>
#include <memory>
#include "exe_graph/runtime/tiling_context.h"
#include "op_tiling.h"

namespace TbeReduce {

struct AutoTilingCompileInfo;
constexpr size_t MAX_INPUT_NUM = 70;
constexpr size_t MAX_AXIS_NUM = 8;
constexpr size_t MAX_OUTPUT_NUM = 10;
class OpInfoImpl {
public:
    explicit OpInfoImpl() = default;
    explicit OpInfoImpl(const AutoTilingCompileInfo *&_compile_info)
    {
        compile_info = _compile_info;
    }

    ~OpInfoImpl() = default;
    void SetInputShape(const std::vector<gert::Shape> *_op_input_ge_shapes);
    void SetInputShape(const gert::Shape * const * _op_input_shapes, size_t num);
    void SetInputShape(const std::vector<std::vector<int64_t>> *_op_input_shapes, bool is_vector = false);
    void SetAxes(const std::vector<int32_t> *_op_axes);
    void SetAxes(const std::vector<int64_t> *_op_axes);
    void AddAxes(const int64_t axis);
    void SetInputType(const ge::DataType *_op_in_type);
    void AddInputShape(const gert::Shape &shape, size_t num);
    void AddOutputShape(const gert::Shape &shape, size_t num);
    const std::vector<std::vector<int64_t>> *GetInputShape() const
    {
        if (op_input_shapes_ptr) {
            return op_input_shapes_ptr;
        }
        if (op_input_shapes.empty()) {
            return nullptr;
        }
        return &op_input_shapes;
    }
    const size_t GetInputNum() const
    {
        return input_num_;
    }
    const size_t GetOutputNum() const;
    const size_t GetMaxDimNum() const
    {
        return max_dim_num_;
    }
    const std::vector<int64_t> *GetAxes() const;
    const int64_t *GetAllAxes(size_t &num) const;
    const ge::DataType *GetInType() const;
    const AutoTilingCompileInfo *GetCompileInfo() const
    {
        return compile_info;
    }

    // Compatible code, please do not use
    const std::vector<std::vector<int64_t>> &GetInputShapeD() const;
    // Compatible code, please do not use
    const std::vector<std::vector<int32_t>> &GetReduceAxesD() const;
    // Compatible code, please do not use
    const ge::DataType &GetInTypeD() const;

private:
    std::vector<std::vector<int64_t>> op_input_shapes;
    std::vector<int64_t> op_axes;
    std::vector<std::vector<int32_t>> op_axes_d;
    const std::vector<std::vector<int64_t>> *op_input_shapes_ptr { nullptr };
    const std::vector<gert::Shape> *op_input_ge_shapes_ptr { nullptr };
    const gert::Shape * const * op_input_shapes_array { nullptr };
    const ge::DataType *op_in_type { nullptr };
    const std::vector<int64_t> *op_axes_ptr { nullptr };
    const AutoTilingCompileInfo *compile_info { nullptr };
    int64_t axes_[MAX_AXIS_NUM];
    size_t axes_num_ { 0 };
    size_t input_num_ { 0 };
    size_t max_dim_num_ { 0 };
};

class OpInfo {
public:
    explicit OpInfo(const std::vector<std::vector<int64_t>>& _op_input_shapes,
                    const ge::DataType& _op_in_type,
                    const std::vector<std::vector<int32_t>>& _op_axes);

    explicit OpInfo(const std::vector<std::vector<int64_t>>& _op_input_shapes,
                    const ge::DataType& _op_in_type);

    explicit OpInfo(const AutoTilingCompileInfo* _compile_info)
    {
        EXECEPTION_CATCH((op_info_impl = std::make_shared<OpInfoImpl>(_compile_info)),
            { HCCL_ERROR("[OpInfo]Construction failed."); op_info_impl = nullptr; });
    }

    // "bool"运算符(可执行if(object){...}的操作判断该OpInfo对象是否有效)
    operator bool() const
    {
        return op_info_impl != nullptr;
    }
    ~OpInfo() = default;

    void SetInputShape(const std::vector<gert::Shape>* _op_input_ge_shapes);
    void SetInputShape(const gert::Shape* const *_op_input_shapes, size_t num);
    void SetInputShape(const std::vector<std::vector<int64_t>>* _op_input_shapes);
    void SetAxes(const std::vector<int64_t>* _op_axes);
    void AddAxes(const int64_t axis);
    void SetInputType(ge::DataType* _op_in_type);
    void AddInputShape(const gert::Shape& shape, size_t num = 1);
    void AddOutputShape(const gert::Shape& shape, size_t num = 1);

    // Compatible code, please do not use
    const std::vector<std::vector<int64_t>>& GetInputShape() const;
    // Compatible code, please do not use
    const std::vector<std::vector<int32_t>>& GetReduceAxes() const;
    // Compatible code, please do not use
    const ge::DataType& GetInType() const;

private:
    friend class OpInfoImplGetter;
    std::shared_ptr<OpInfoImpl> op_info_impl;
};
}  // namespace TbeReduce

#endif  // OPS_BUILT_IN_OP_TILING_VECTOR_OP_INFO_H
