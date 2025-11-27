/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "llt_hccl_stub.h"
#include "common/hyper_status.h"
#include "common/checker.h"
#include "exe_graph/lowering/value_holder.h"
#include "exe_graph/lowering/frame_selector.h"
#include "exe_graph/lowering/lowering_global_data.h"
#include "exe_graph/lowering/generate_exe_graph.h"
#include "exe_graph/lowering/buffer_pool.h"
#include "register/node_converter_registry.h"
#include "register/kernel_registry.h"
#include "register/kernel_registry_impl.h"
#include "register/op_impl_registry.h"
#include "external/register/op_tiling_info.h"
#include "register/op_impl_kernel_registry.h"
#include "exe_graph/lowering/tiling_parse_context_builder.h"
#include "exe_graph/lowering/tiling_context_builder.h"
#include "exe_graph/runtime/compute_node_info.h"
#include "exe_graph/lowering/shape_utils.h"
#include "register/op_tiling_attr_utils.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/math_util.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/node.h"
#include "graph/op_desc.h"
#include <stack>
#include <securec.h>
#include <cstdint>
#include "common/ge_common/util.h"
#include "graph/debug/ge_log.h"
#include "graph/ge_error_codes.h"
#include "register/op_impl_registry_base.h"
#include "llt_hccl_stub_kernel_run_ctx_faker.h"

void DlogErrorInner(int module_id, const char *fmt, ...) {
    return;
}

int32_t DlogSetAttr(LogAttr logAttrInfo) {
    return 0;
}
void DlogWarnInner(int module_id, const char *fmt, ...) {
    return;
}

void DlogInfoInner(int module_id, const char *fmt, ...) {
    return;
}

void DlogDebugInner(int module_id, const char *fmt, ...) {
    return;
}

void DlogEventInner(int module_id, const char *fmt, ...) {
    return;
}

namespace gert {
template<typename T, typename... Args>
template <typename T>
struct ComGraphMakeUniq {
  using unique_object = std::unique_ptr<T>;
};

template <typename T>
struct ComGraphMakeUniq<T[]> {
  using unique_array = std::unique_ptr<T[]>;
};

template <typename T, size_t B>
struct ComGraphMakeUniq<T[B]> {
  struct invalid_type { };
};

template <typename T, typename... Args>
static inline typename ComGraphMakeUniq<T>::unique_object ComGraphMakeUnique(Args &&... args) {
  using T_nc = typename std::remove_const<T>::type;
  return std::unique_ptr<T>(new (std::nothrow) T_nc(std::forward<Args>(args)...));
}

template <typename T>
static inline typename ComGraphMakeUniq<T>::unique_array ComGraphMakeUnique(const size_t num) {
  return std::unique_ptr<T>(new (std::nothrow) typename std::remove_extent<T>::type[num]());
}

template <typename T, typename... Args>
static inline typename ComGraphMakeUniq<T>::invalid_type ComGraphMakeUnique(Args &&...) = delete;

namespace {
uint32_t GeMemcpy(uint8_t *dst_ptr, size_t dst_size, const uint8_t *src_ptr, const size_t src_size) {
  return 0;
}
std::unique_ptr<uint8_t[]> CreateComputeNodeInfoImpl(const std::unique_ptr<uint8_t[]> &attr_buf,
                                                     const size_t attr_size,
                                                     const ge::NodePtr &node,
                                                     gert::bg::BufferPool &buffer_pool,
                                                     size_t &total_size) {
    const auto ir_input_num = node->GetOpDesc()->GetIrInputs().size();
    const auto input_num = node->GetInDataNodesAndAnchors().size();
    const auto output_num = node->GetAllOutDataAnchorsSize();
    HCCL_INFO("node: %s(%s), ir_input_num:%zu, input_num:%zu, output_num:%u.",
          node->GetName().c_str(), node->GetType().c_str(), ir_input_num, input_num, output_num);
    ComputeNodeInfo::CalcSize(ir_input_num, input_num, output_num, total_size);
    GE_ASSERT_TRUE(!ge::AddOverflow(total_size, attr_size, total_size));
    auto compute_node_info_holder = ComGraphMakeUnique<uint8_t[]>(total_size);

    auto node_name = buffer_pool.AddStr(node->GetName().c_str());
    auto node_type = buffer_pool.AddStr(node->GetType().c_str());
    auto compute_node_info = ge::PtrToPtr<uint8_t, ComputeNodeInfo>(compute_node_info_holder.get());
    compute_node_info->Init(ir_input_num, input_num, output_num,
                            ge::PtrToPtr<void, ge::char_t>(ge::ValueToPtr(node_name)),
                            ge::PtrToPtr<void, ge::char_t>(ge::ValueToPtr(node_type)));

    auto ret = InitInputInstanceInfo(node, *compute_node_info);
    ret = InitCompileTimeTD(node, *compute_node_info);
    auto attr = compute_node_info->MutableAttrs();
    const auto offset = ge::PtrToPtr<RuntimeAttrs, uint8_t>(attr) - compute_node_info_holder.get();
    if (static_cast<size_t>(offset) > total_size) {
      return nullptr;
    }
    ret = GeMemcpy(ge::PtrToPtr<RuntimeAttrs, uint8_t>(attr), (total_size - offset), attr_buf.get(), attr_size);
    return compute_node_info_holder;
}

std::unique_ptr<uint8_t[]> CreateAttrBuffer(const std::vector<std::vector<uint8_t>> &attrs, size_t &total_size) {
  typedef struct {
  size_t attr_num;
  uint8_t reserved_[40];  // Reserved field, 32+8, do not directly use when only 8-byte left
  size_t offset[0];
} RuntimeAttrsDef;
  total_size = sizeof(RuntimeAttrsDef);
  size_t offset_size = 0U;
    if (ge::MulOverflow(sizeof(size_t), attrs.size(), offset_size)) {
    return nullptr;
  }
  if (ge::AddOverflow(total_size, offset_size, total_size)) {
    return nullptr;
  }
  for (const auto &attr : attrs) {
    if (ge::AddOverflow(total_size, attr.size(), total_size)) {
      return nullptr;
    }
  }
  auto attr_holder = ComGraphMakeUnique<uint8_t[]>(total_size);
  auto attr_def = ge::PtrToPtr<uint8_t, RuntimeAttrsDef>(attr_holder.get());
  attr_def->attr_num = attrs.size();
  memset(attr_def->reserved_, 0, sizeof(attr_def->reserved_));
  size_t current_offset = sizeof(RuntimeAttrsDef) + sizeof(size_t) * attr_def->attr_num;
  auto attr_pos = attr_holder.get();
  for (size_t i = 0; i < attrs.size(); ++i) {
    attr_def->offset[i] = current_offset;
    const auto ret = GeMemcpy(attr_pos + current_offset, total_size - current_offset,
        attrs[i].data(), attrs[i].size());
    current_offset += attrs[i].size();
  }
  return attr_holder;
}
}

ge::NodePtr KernelRunContextBuilder::MakeNode(const ge::OpDescPtr &op_desc) {
  const auto node_id = op_desc->GetId();
  graph_ = std::make_shared<ge::ComputeGraph>("tmp");
  auto fake_node = graph_->AddNode(op_desc);
  for (size_t i = 0UL; i < op_desc->GetAllInputsSize(); ++i) {
    const auto input_desc = op_desc->GetInputDesc(i);
    auto op_data = ge::OpDescBuilder(std::to_string(i), "Data").AddInput("x").AddOutput("y").Build();
    auto data_node = graph_->AddNode(op_data);
    ge::GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), fake_node->GetInDataAnchor(i));
  }
  op_desc->SetId(node_id);
  return fake_node;
}

TilingContextBuilder &TilingContextBuilder::CompileInfo(void *compile_info) {
  compile_info_ = compile_info;
  return *this;
}
TilingContextBuilder &TilingContextBuilder::PlatformInfo(void *platform_info) {
  platform_info_ = platform_info;
  return *this;
}
TilingContextBuilder &TilingContextBuilder::TilingData(void *tiling_data) {
  outputs_[TilingContext::kOutputTilingData] = tiling_data;
  return *this;
}
TilingContextBuilder &TilingContextBuilder::Workspace(ContinuousVector *workspace) {
  outputs_[TilingContext::kOutputWorkspace] = workspace;
  return *this;
}

ge::graphStatus TilingContextBuilder::BuildRTInputTensors(const ge::Operator &op) {
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus TilingContextBuilder::BuildRTOutputShapes(const ge::Operator &op) {
  return ge::GRAPH_SUCCESS;
}
KernelContextHolder TilingContextBuilder::Build(const ge::Operator &op) {
    KernelContextHolder holder;
    auto node =  ge::OpDescUtils::GetOpDescFromOperator(op);
    std::vector<void *> context_inputs;
    auto ret = BuildRTInputTensors(op);
    ret = BuildRTOutputShapes(op);
    for (const auto &input_holder : rt_tensor_holders_) {
      context_inputs.emplace_back(input_holder.get());
    }
    context_inputs.emplace_back(compile_info_);
    context_inputs.emplace_back(platform_info_);
    context_inputs.emplace_back(nullptr);

    return base_builder_.Inputs(context_inputs).Outputs(outputs_).Build(node);
}
ge::graphStatus TilingData::AppendConvertedAttrVal(const RuntimeAttrs *attrs, const size_t attr_index,
                                                   const AttrDataType src_type, const AttrDataType dst_type) {
  return ge::GRAPH_SUCCESS;
}

const Shape g_vec_1_shape;
int64_t GetInputCDim(TilingContext *kernel_context, const size_t index) {
  return 0;
}

class OpImplRegisterV2Impl {
 public:
  OpImplRegistry::OpType op_type;
  OpImplRegistry::OpImplFunctions functions;
  bool is_private_attr_registered = false;
};

OpImplRegistry &OpImplRegistry::GetInstance() {
  static OpImplRegistry instance;
  return instance;
}

OpImplRegistry::OpImplFunctionsV2 &OpImplRegistry::CreateOrGetOpImpl(const ge::char_t *op_type) {
  (void)reserved_;
  return types_to_impl_[op_type];
}
const OpImplRegistry::OpImplFunctionsV2 *OpImplRegistry::GetOpImpl(const ge::char_t *op_type) const {
  const auto iter = types_to_impl_.find(op_type);
  if (iter == types_to_impl_.end()) {
    return nullptr;
  }
  return &iter->second;
}
const OpImplRegistry::PrivateAttrList &OpImplRegistry::GetPrivateAttrs(const ge::char_t *op_type) const {
  const auto op_impl_ptr = GetOpImpl(op_type);
  if (op_impl_ptr == nullptr) {
    static OpImplRegistry::PrivateAttrList emptyPrivateAttr;
    return emptyPrivateAttr;
  }
  return op_impl_ptr->private_attrs;
}

ge::graphStatus ComputeNodeInfo::CalcSize(const size_t ir_inputs_num, const size_t inputs_num,
                                          const size_t outputs_num, size_t &total_size) {
  size_t ir_inputs_size;
  size_t inputs_size;
  size_t outputs_size;

  GE_ASSERT_TRUE(!ge::MulOverflow(sizeof(gert::AnchorInstanceInfo), ir_inputs_num, ir_inputs_size));
  GE_ASSERT_TRUE(!ge::MulOverflow(sizeof(CompileTimeTensorDesc), inputs_num, inputs_size));
  GE_ASSERT_TRUE(!ge::MulOverflow(sizeof(CompileTimeTensorDesc), outputs_num, outputs_size));

  total_size = sizeof(ComputeNodeInfo);
  GE_ASSERT_TRUE(!ge::AddOverflow(total_size, ir_inputs_size, total_size));
  GE_ASSERT_TRUE(!ge::AddOverflow(total_size, inputs_size, total_size));
  GE_ASSERT_TRUE(!ge::AddOverflow(total_size, outputs_size, total_size));
  return ge::GRAPH_SUCCESS;
}
void ComputeNodeInfo::Init(const size_t ir_inputs_num, const size_t inputs_num, const size_t outputs_num,
                           const char *node_name, const char *node_type) {
  ir_inputs_num_ = ir_inputs_num;
  inputs_num_ = inputs_num;
  outputs_num_ = outputs_num;
  node_name_ = node_name;
  node_type_ = node_type;
  memset(reserved_, 0, sizeof(reserved_));
}

RuntimeAttrs *ComputeNodeInfo::MutableAttrs() const {
  return const_cast<RuntimeAttrs *>(GetAttrs());
}

// send/recv
ge::graphStatus CalcAlignedSizeByShape(const Shape &shape, ge::DataType data_type, uint64_t &ret_tensor_size) {
  constexpr uint64_t kAlignBytes = 32U;
  auto shape_size = shape.GetShapeSize();
  int64_t cal_size = 0;
  if (data_type == 13) {
    uint32_t type_size = 0U;
    if (ge::MulOverflow(shape_size, static_cast<int64_t>(type_size), cal_size)) {
      return ge::GRAPH_FAILED;
    }
  } else {
    cal_size = ge::GetSizeInBytes(shape_size, data_type);
  }
  if (cal_size < 0) {
    return ge::GRAPH_FAILED;
  }
 
  // 不可能溢出，因为ret最大值也只有int64的最大值
  ret_tensor_size = ge::RoundUp(static_cast<uint64_t>(cal_size), kAlignBytes) + kAlignBytes;
  return ge::GRAPH_SUCCESS;
}

GertMemBlock *AllocatorFaker::Malloc(size_t size) {
    GertMemBlock *block = reinterpret_cast<GertMemBlock *>(new GertMemBlockFaker(malloc(size)));
    return block;
}
void AllocatorFaker::Free(GertMemBlock *block) {
    free(block->GetAddr());
    delete block;
}

namespace bg {
const char *BufferPool::GetBufById(const BufId id) const {
    for (const auto &buf_and_id : bufs_to_id_) {
      if (buf_and_id.second == id) {
        return buf_and_id.first.c_str();
      }
    }
    for (const auto &buf_and_id : large_bufs_to_id_) {
      if (buf_and_id.second == id) {
        return buf_and_id.first.c_str();
      }
    }
    return nullptr;
}

bool AppendAttr(const ge::AnyValue &attr, std::vector<std::vector<uint8_t>> &attrs) {
    return true;
}
std::unique_ptr<uint8_t[]> CreateComputeNodeInfo(const ge::NodePtr &node, BufferPool &buffer_pool, size_t &total_size) {
    size_t attr_size;
    std::vector<std::vector<uint8_t>> runtime_attrs;
    std::vector<ge::AnyValue> runtime_attrs_list;
    GetAllIrAttrs(node, runtime_attrs);
    for (auto &runtime_attr : runtime_attrs_list) {
      AppendAttr(runtime_attr, runtime_attrs);
    }
    const auto attr_buf = CreateAttrBuffer(runtime_attrs, attr_size);
    return CreateComputeNodeInfoImpl(attr_buf, attr_size, node, buffer_pool, total_size);
}

BufferPool::BufId BufferPool::AddLargeBuf(std::string &&str) {
  auto id = id_generator_++;
  large_bufs_to_id_.emplace_back(std::move(str), id);
  return id;
}
BufferPool::BufId BufferPool::AddBuf(std::string &&str) {
  auto res = bufs_to_id_.emplace(std::move(str), id_generator_);
  if (res.second) {
    ++id_generator_;
  }
  return res.first->second;
}
BufferPool::BufId BufferPool::AddStr(const char *data) {
  size_t len = strlen(data) + 1;
  if (len >= 1024U * 1024U) {
    return AddLargeBuf(std::string(data, len));
  }
  return AddBuf(std::string(data, len));
}

}  // namespace bg
}  // namespace gert
ge::graphStatus GetOperatorAttrValue(const ge::Operator &op, const char *attr_name, const char *attr_dtype,
                                    optiling::AttrDataPtr &attr_data_ptr, const char *target_dtype) {
  return ge::GRAPH_SUCCESS;
}

