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
#include "base/err_msg.h"
#include "base/err_mgr.h"
#include <stack>
#include <securec.h>
#include <cstdint>
#include "common/ge_common/util.h"
#include "graph/debug/ge_log.h"
#include "graph/ge_error_codes.h"
#include "register/op_impl_registry_base.h"
#include "base/context_builder/context_holder_builder.h"
#include "common/ge_common/util.h"
#include "graph/debug/ge_util.h"
#include "base/context_builder/op_tiling_parse_context_builder.h"
#include "base/context_builder/op_tiling_context_builder.h"
#include "graph/operator_reg.h"
#include "exe_graph/runtime/storage_shape.h"
#include "base/context_builder/op_context_builder_impl.h"

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

namespace ge {
bool AscendString::operator<(const AscendString &d) const {
  if ((name_ == nullptr) && (d.name_ == nullptr)) {
    return false;
  } else if (name_ == nullptr) {
    return true;
  } else if (d.name_ == nullptr) {
    return false;
  } else {
    return (*name_) < (*(d.name_));
  }
}

bool AscendString::operator>(const AscendString &d) const {
  if ((name_ == nullptr) && (d.name_ == nullptr)) {
    return false;
  } else if (name_ == nullptr) {
    return false;
  } else if (d.name_ == nullptr) {
    return true;
  } else {
    return (*name_) > (*(d.name_));
  }
}

bool AscendString::operator==(const AscendString &d) const {
  if ((name_ == nullptr) && (d.name_ == nullptr)) {
    return true;
  } else if (name_ == nullptr) {
    return false;
  } else if (d.name_ == nullptr) {
    return false;
  } else {
    return (*name_) == (*(d.name_));
  }
}

bool AscendString::operator<=(const AscendString &d) const {
  if (name_ == nullptr) {
    return true;
  } else if (d.name_ == nullptr) {
    return false;
  } else {
    return (*name_) <= (*(d.name_));
  }
}

bool AscendString::operator>=(const AscendString &d) const {
  if (d.name_ == nullptr) {
    return true;
  } else if (name_ == nullptr) {
    return false;
  } else {
    return (*name_) >= (*(d.name_));
  }
}

bool AscendString::operator!=(const AscendString &d) const {
  if ((name_ == nullptr) && (d.name_ == nullptr)) {
    return false;
  } else if (name_ == nullptr) {
    return true;
  } else if (d.name_ == nullptr) {
    return true;
  } else {
    return (*name_) != (*(d.name_));
  }
}

OperatorCreatorRegister::OperatorCreatorRegister(const std::string &operator_type, OpCreator const &op_creator) {}
OperatorCreatorRegister::OperatorCreatorRegister(const char_t *const operator_type, OpCreatorV2 const &op_creator) {}
AscendString::AscendString(const char_t *const name) {
}
GE_FUNC_HOST_VISIBILITY Operator::Operator(const AscendString &name, const AscendString &type) {
}
void Operator::InputRegister(const char_t *name, const char_t *datatype_symbol) {
}
void Operator::OutputRegister(const char_t *name, const char_t *datatype_symbol) {
}
}

namespace gert {
template<typename T, typename... Args>
static inline std::shared_ptr<T> ComGraphMakeShared(Args &&...args) {
  using T_nc = typename std::remove_const<T>::type;
  std::shared_ptr<T> ret = nullptr;
  try {
    ret = std::make_shared<T_nc>(std::forward<Args>(args)...);
  } catch (const std::bad_alloc &) {
    ret = nullptr;
    GELOGE(ge::FAILED, "Make shared failed");
  }
  return ret;
}
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
ge::graphStatus InitInputInstanceInfo(const ge::NodePtr &node, ComputeNodeInfo &compute_node_info) {
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus InitCompileTimeTD(const ge::NodePtr &node, ComputeNodeInfo &compute_node_info) {
  return ge::GRAPH_SUCCESS;
}
uint32_t GeMemcpy(uint8_t *dst_ptr, size_t dst_size, const uint8_t *src_ptr, const size_t src_size) {
  return 0;
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

// No.1 HyperStatus
HyperStatus HyperStatus::ErrorStatus(const ge::char_t *message, ...) {
    HyperStatus status;
    return status;
}

HyperStatus HyperStatus::Success() {
    return {};
}

// No.2 NodeConverterRegister
NodeConverterRegistry &NodeConverterRegistry::GetInstance() {
    static NodeConverterRegistry registry;
    return registry;
}

void NodeConverterRegistry::Register(const string &func_name,
                                     const NodeConverterRegistry::ConverterRegisterData &data) {
    names_to_register_data_[func_name] = data;
}

NodeConverterRegister::NodeConverterRegister(const char *lower_func_name,
                                             NodeConverterRegistry::NodeConverter func) noexcept {
    NodeConverterRegistry::GetInstance().Register(lower_func_name, {func, -1});
}

NodeConverterRegister::NodeConverterRegister(const char *lower_func_name, int32_t require_placement,
                                             NodeConverterRegistry::NodeConverter func) noexcept {
    NodeConverterRegistry::GetInstance().Register(lower_func_name, {func, require_placement});
}

// No.3 KernelRegistryImpl
KernelRegistryImpl &KernelRegistryImpl::GetInstance() {
    static KernelRegistryImpl registry;
    return registry;
}

void KernelRegistryImpl::RegisterKernel(std::string kernel_type, KernelInfo kernel_infos) {
    kernel_infos_[std::move(kernel_type)] = std::move(kernel_infos);
}

class KernelRegisterData {
 public:
  explicit KernelRegisterData(const ge::char_t *kernel_type);

  KernelRegistry::KernelFuncs &GetFuncs() {
    return funcs_;
  }

  std::string &GetKernelType() {
    return kernel_type_;
  }

 private:
  std::string kernel_type_;
  KernelRegistry::KernelFuncs funcs_;
};

KernelRegisterV2 &KernelRegisterV2::OutputsCreator(KernelRegistry::CreateOutputsFunc func){
  if (register_data_ != nullptr) {
    register_data_->GetFuncs().outputs_creator = func;
  }
  return *this;
}

KernelRegisterV2::KernelRegisterV2(const char *kernel_type)
    : register_data_(new(std::nothrow) KernelRegisterData(kernel_type)) {}
KernelRegisterV2::~KernelRegisterV2() = default;
KernelRegisterV2 &KernelRegisterV2::RunFunc(KernelRegistry::KernelFunc func) {
  if (register_data_ != nullptr) {
    register_data_->GetFuncs().run_func = func;
  }
  return *this;
}

KernelRegisterV2 &KernelRegisterV2::TracePrinter(KernelRegistry::TracePrinter func) {
  if (register_data_ != nullptr) {
    register_data_->GetFuncs().trace_printer = func;
  }
  return *this;
}

KernelRegisterV2::KernelRegisterV2(const KernelRegisterV2 &other) : register_data_(nullptr) {
  auto register_data = other.register_data_.get();
  if (register_data == nullptr) {
    return;
  }
  KernelRegistryImpl::GetInstance().RegisterKernel(register_data->GetKernelType(), {register_data->GetFuncs(), ""});
}

KernelRegisterData::KernelRegisterData(const ge::char_t *kernel_type) : kernel_type_(kernel_type) {
    return;
}

const KernelRegistryImpl::KernelFuncs *KernelRegistryImpl::FindKernelFuncs(const std::string &kernel_type) const {
    auto iter = kernel_infos_.find(kernel_type);
    if (iter == kernel_infos_.end()) {
        return nullptr;
    }
    return &iter->second.func;
}

const KernelRegistry::KernelInfo *KernelRegistryImpl::FindKernelInfo(const std::string &kernel_type) const {
  auto iter = kernel_infos_.find(kernel_type);
  if (iter == kernel_infos_.end()) {
    return nullptr;
  }
  return &iter->second;
}

// No.5 LoweringGlobalData
bg::ValueHolderPtr LoweringGlobalData::GetOrCreateL2Allocator(int64_t logic_stream_id, const AllocatorDesc desc) {
	(void)logic_stream_id;
	bg::ValueHolderPtr init_selected_allocator = nullptr;
    auto placement_holder = bg::ValueHolder::CreateConst(&desc.placement, sizeof(desc.placement));
    auto memory_type_holder = bg::ValueHolder::CreateConst(&desc.usage, sizeof(desc.usage));
    auto created_allocator =
        bg::ValueHolder::CreateSingleDataOutput("CreateAllocator", {placement_holder, memory_type_holder});
    init_selected_allocator = created_allocator;
    return init_selected_allocator;
}

bg::ValueHolderPtr LoweringGlobalData::GetStreamById(int64_t logic_stream_id) const {
	(void)logic_stream_id;
	return bg::ValueHolder::CreateSingleDataOutput("Stream", {});
}

bg::ValueHolderPtr LoweringGlobalData::GetL1Allocator(const AllocatorDesc &desc) const {
    bg::ValueHolderPtr init_allocator = nullptr;
    return init_allocator;
}

TilingParseContextBuilder &TilingParseContextBuilder::CompileJson(const ge::char_t *compile_json) {
  compile_json_ = const_cast<ge::char_t *>(compile_json);
  return *this;
}

TilingParseContextBuilder &TilingParseContextBuilder::PlatformInfo(void *platform_info) {
  platform_info_ = platform_info;
  return *this;
}

TilingParseContextBuilder &TilingParseContextBuilder::CompileInfoCreatorFunc(
    OpImplKernelRegistry::CompileInfoCreatorFunc create_func) {
  create_func_ = create_func;
  return *this;
}

TilingParseContextBuilder &TilingParseContextBuilder::CompileInfoDeleterFunc(
    OpImplKernelRegistry::CompileInfoDeleterFunc delete_func) {
  delete_func_ = delete_func;
  return *this;
}

template class OpContextBuilderBase<OpTilingContextBuilder>;
template class OpContextBuilderBase<OpTilingParseContextBuilder>;

template<typename T>
OpContextBuilderBase<T>::OpContextBuilderBase() : impl_() {}
template<typename T>
OpContextBuilderBase<T>::~OpContextBuilderBase() = default;

template<typename T>
T &OpContextBuilderBase<T>::OpType(const AscendString &op_type) {
  return static_cast<T &>(*this);
}

template<typename T>
T &OpContextBuilderBase<T>::OpName(const AscendString &op_name) {
  return static_cast<T &>(*this);
}

template<typename T>
T &OpContextBuilderBase<T>::IONum(size_t input_ir_num, size_t output_ir_num) {
  return static_cast<T &>(*this);
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
TilingContextBuilder &TilingContextBuilder::SpaceRegistry(const gert::OpImplSpaceRegistryPtr &space_registry) {
  (void) space_registry;
  return *this;
}

void *ContextHolderVoid::GetContext() const{
  int i = 0;
  return &i;
}

ContextHolderVoid::ContextHolderVoid() = default;
ContextHolderVoid::~ContextHolderVoid() = default;
ContextHolderVoid::ContextHolderVoid(ContextHolderVoid &&other) noexcept{
}
ContextHolderVoid &ContextHolderVoid::operator=(ContextHolderVoid &&other) noexcept{
  return *this;
}

ge::graphStatus ContextBuilderImpl::CreateComputeNodeInfo(ContextHolderImpl &holder) {
  return ge::GRAPH_SUCCESS;
}
ge::graphStatus ContextBuilderImpl::BuildCtx(ContextHolderImpl &holder) {
  return ge::GRAPH_SUCCESS;
}
class OpTilingParseContextBuilderImpl : public ContextBuilderImpl {
 public:
  OpTilingParseContextBuilderImpl() : ContextBuilderImpl() {}
  ~OpTilingParseContextBuilderImpl() override = default;

  std::unique_ptr<ContextHolderImpl> BuildTilingParseContext() {
    std::unique_ptr<ContextHolderImpl> holder = std::make_unique<ContextHolderImpl>();
    return holder;
  }
};

OpTilingParseContextBuilder::OpTilingParseContextBuilder() {}
OpTilingParseContextBuilder::~OpTilingParseContextBuilder() {};

OpTilingParseContextBuilder &OpTilingParseContextBuilder::InputTensorDesc(size_t index, ge::DataType dtype,
    ge::Format origin_format, ge::Format storage_format, const gert::ExpandDimsType &expand_dims_type)
{
    return *this;
}

OpTilingParseContextBuilder &OpTilingParseContextBuilder::OutputTensorDesc(size_t index, ge::DataType dtype,
    ge::Format origin_format, ge::Format storage_format, const gert::ExpandDimsType &expand_dims_type)
{
    return *this;
}

OpTilingParseContextBuilder &OpTilingParseContextBuilder::CompiledJson(const ge::char_t *compiled_json) {
  return *this;
}

OpTilingParseContextBuilder &OpTilingParseContextBuilder::CompiledInfo(const void *compile_info) {
  return *this;
}

OpTilingParseContextBuilder &OpTilingParseContextBuilder::PlatformInfo(const void *platform_info) {
  return *this;
}

ContextHolder<TilingParseContext> OpTilingParseContextBuilder::Build() {
  return ContextHolder<TilingParseContext>();
}

class OpTilingContextBuilderImpl : public ContextBuilderImpl {
 public:
  OpTilingContextBuilderImpl() : ContextBuilderImpl() {}
  ~OpTilingContextBuilderImpl() override = default;

  std::unique_ptr<ContextHolderImpl> BuildTilingContext() {
    std::unique_ptr<ContextHolderImpl> holder = std::make_unique<ContextHolderImpl>();
    return holder;
  }
};
OpTilingContextBuilder::OpTilingContextBuilder(){}
OpTilingContextBuilder::~OpTilingContextBuilder() {};

OpTilingContextBuilder &OpTilingContextBuilder::CompileInfo(const void *compile_info) {
  return *this;
}

OpTilingContextBuilder &OpTilingContextBuilder::PlatformInfo(const void *platform_info) {
  return *this;
}
OpTilingContextBuilder &OpTilingContextBuilder::Deterministic(int32_t deterministic) {
  return *this;
}

OpTilingContextBuilder &OpTilingContextBuilder::TilingData(const gert::TilingData *tiling_data,
                                                           gert::Chain::Deleter deleter) {
  return *this;
}

OpTilingContextBuilder &OpTilingContextBuilder::TilingDataSize(size_t tiling_data_size) {
  return *this;
}

OpTilingContextBuilder &OpTilingContextBuilder::Workspace(const gert::ContinuousVector *workspace) {
  return *this;
}

OpTilingContextBuilder &OpTilingContextBuilder::InputTensors(const std::vector<gert::Tensor *> &inputs) {
  return *this;
}

OpTilingContextBuilder &OpTilingContextBuilder::OutputTensors(const std::vector<gert::Tensor *> &outputs) {
  return *this;
}

ContextHolder<TilingContext> OpTilingContextBuilder::Build() {
  return ContextHolder<TilingContext>();
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
const std::map<OpImplRegisterV2::OpType, OpImplRegistry::OpImplFunctionsV2> &OpImplRegistry::GetAllTypesToImpl() const {
  return types_to_impl_;
}
std::map<OpImplRegisterV2::OpType, OpImplRegistry::OpImplFunctionsV2> &OpImplRegistry::GetAllTypesToImpl() {
  return types_to_impl_;
}

OpImplRegisterV2::OpImplRegisterV2(const ge::char_t *op_type) : impl_(new(std::nothrow) OpImplRegisterV2Impl) {
  if (impl_ != nullptr) {
    impl_->op_type = op_type;
    impl_->functions.infer_shape = nullptr;
    impl_->functions.infer_shape_range = nullptr;
    impl_->functions.infer_datatype = nullptr;
    impl_->functions.inputs_dependency = 0U;

    // two fields controlled by tiling func
    impl_->functions.tiling = nullptr;
    impl_->functions.max_tiling_data_size = std::numeric_limits<size_t>::max();

    // 3 fields controlled by tiling_parse func
    impl_->functions.tiling_parse = nullptr;
    impl_->functions.compile_info_creator = nullptr;
    impl_->functions.compile_info_deleter = nullptr;

     (void)OpImplRegistry::GetInstance().CreateOrGetOpImpl(op_type);
  }
}

OpImplRegisterV2 &OpImplRegisterV2::Tiling(OpImplKernelRegistry::TilingKernelFunc tiling_func,
                                           size_t max_tiling_data_size) {
  if (impl_ != nullptr) {
    impl_->functions.tiling = tiling_func;
    impl_->functions.max_tiling_data_size = max_tiling_data_size;
  }
  return *this;
}

OpImplRegisterV2 &OpImplRegisterV2::TilingParse(KernelRegistry::KernelFunc tiling_parse_func,
                                                OpImplKernelRegistry::CompileInfoCreatorFunc creator_func,
                                                OpImplKernelRegistry::CompileInfoDeleterFunc deleter_func) {
  if (impl_ != nullptr) {
    impl_->functions.tiling_parse = tiling_parse_func;
    impl_->functions.compile_info_creator = creator_func;
    impl_->functions.compile_info_deleter = deleter_func;
  }
  return *this;
}

OpImplRegisterV2::~OpImplRegisterV2() = default;
OpImplRegisterV2::OpImplRegisterV2(const OpImplRegisterV2 &register_data) {
}
OpImplRegisterV2::OpImplRegisterV2(OpImplRegisterV2 &&register_data) noexcept {
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
// No.6 ValueHolder

namespace bg {
 // namespace
std::atomic<int64_t> ValueHolder::id_generator_{0};
ValueHolder::~ValueHolder() = default;

ValueHolder::ValueHolder()
    : id_(id_generator_++), type_(ValueHolderType::kValueHolderTypeEnd), index_(0), placement_(0) {}

ValueHolderPtr ValueHolder::CreateConst(const void *data, size_t size, bool is_string)
{
    ValueHolderPtr holder = std::shared_ptr<ValueHolder>(new (std::nothrow) ValueHolder());
    return holder;
}

ValueHolderPtr ValueHolder::CreateError(const char *fmt, va_list arg) {
    auto value_holder = std::shared_ptr<ValueHolder>(new (std::nothrow) ValueHolder());
    return value_holder;
}

ValueHolderPtr ValueHolder::CreateSingleDataOutput(const char *node_type, const vector<ValueHolderPtr> &inputs)
{
    auto output_data = std::shared_ptr<ValueHolder>(new (std::nothrow) ValueHolder());
    return output_data;
}

GraphFrame * ValueHolder::GetCurrentFrame()
{
    return nullptr;
}

std::vector<ValueHolderPtr> ValueHolder::CreateDataOutput(const char *node_type,
    const std::vector<ValueHolderPtr> &inputs, size_t out_count)
{
    std::vector<ValueHolderPtr> holders;
    return holders;
}

void ValueHolder::SetPlacement(const int32_t &placement) {
    placement_ = placement;
}

bool ValueHolder::IsOk() const noexcept {
    return error_msg_ == nullptr;
}

ValueHolderPtr ValueHolder::CreateVoidGuarder(const char *node_type, const ValueHolderPtr &resource,
    const vector<ValueHolderPtr> &args)
{
    auto guarder = std::shared_ptr<ValueHolder>(new (std::nothrow) ValueHolder());
    return guarder;
}

ValueHolderPtr ValueHolder::CreateMateFromNode(ge::FastNode *node, int32_t index, ValueHolderType type) {
  auto holder = std::shared_ptr<ValueHolder>(new (std::nothrow) ValueHolder());
    return holder;  
}

ValueHolderPtr DevMemValueHolder::CreateMateFromNode(ge::FastNode *node, int32_t index, ValueHolderType type) {
    auto holder = std::shared_ptr<ValueHolder>(new (std::nothrow) ValueHolder());
    return holder;
}

DevMemValueHolderPtr DevMemValueHolder::CreateSingleDataOutput(const char *node_type,
                                                               const std::vector<ValueHolderPtr> &inputs,
                                                               int64_t logic_stream_id) {
    auto holder = std::shared_ptr<DevMemValueHolder>(new (std::nothrow) DevMemValueHolder(0));
    return holder;
}

std::vector<DevMemValueHolderPtr> DevMemValueHolder::CreateDataOutput(const char *node_type,
                                                                      const std::vector<ValueHolderPtr> &inputs,
                                                                      size_t out_count, int64_t logic_stream_id) {
  std::vector<DevMemValueHolderPtr> holders;
  return holders;
}

// No.6 GenerateExeGraph
GenerateExeGraph::ExeGraphGenerator GenerateExeGraph::generator_;
static std::vector<ValueHolderPtr> InferShape(const ge::NodePtr &node, const std::vector<ValueHolderPtr> &shapes) {
    std::vector<ValueHolderPtr> holders;
    return holders;
}

static std::vector<ValueHolderPtr> AllocOutputMemory(TensorPlacement placement, const ge::NodePtr &node,
    const std::vector<ValueHolderPtr> &output_sizes, LoweringGlobalData &global_data) {
    std::vector<ValueHolderPtr> holders;
    return holders;
}

static std::vector<ValueHolderPtr> CalcTensorSize(const ge::NodePtr &node,
    const std::vector<ValueHolderPtr> &output_shapes) {
    std::vector<ValueHolderPtr> holders;
    size_t outputSize = output_shapes.size();
    ValueHolderPtr outputHolder;
    for (size_t i = 0; i < outputSize; i++) {
        holders.push_back(outputHolder);
    }
    return holders;
}

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
bool GetAllIrAttrs(const ge::NodePtr &node, std::vector<std::vector<uint8_t>> &runtime_attrs) {
  return true;
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

std::vector<ValueHolderPtr> FrameSelector::OnInitRoot(const std::function<std::vector<ValueHolderPtr>()> &builder) {
  std::vector<ValueHolderPtr> init_node_outputs;
  return init_node_outputs;
}

}  // namespace bg

}  // namespace gert


namespace optiling {
namespace utils {
class OpRunInfoImpl {
public:
  OpRunInfoImpl() = default;
  ~OpRunInfoImpl() = default;

  OpRunInfoImpl(const uint32_t &block_dim, const bool &clear_atomic, const uint64_t &tiling_key)
          : block_dim_(block_dim),
            clear_atomic_(clear_atomic),
            tiling_key_(tiling_key),
            addr_base_(nullptr),
            max_size_(0),
            offset_(0),
            tiling_cond_(-1) {}

  void SetBlockDim(const uint32_t &block_dim) { block_dim_ = block_dim; }

  uint32_t GetBlockDim() const { return block_dim_; }

  void AddWorkspace(const int64_t &workspace) { workspaces_.push_back(workspace); }

  size_t GetWorkspaceNum() const { return workspaces_.size(); }

  ge::graphStatus GetWorkspace(const size_t &idx, int64_t &workspace) const {
    if ((!workspaces_.empty()) && (idx < workspaces_.size())) {
      workspace = workspaces_[idx];
      return ge::GRAPH_SUCCESS;
    }
    return ge::GRAPH_FAILED;
  }

  void GetAllWorkspaces(std::vector<int64_t> &workspaces) const { workspaces = workspaces_; }

  const std::vector<int64_t> &GetAllWorkspaces() const { return workspaces_; }

  void SetWorkspaces(const std::vector<int64_t> &workspaces) { workspaces_ = workspaces; }

  void AddTilingData(const char *value, const size_t size) {
    if (addr_base_ == nullptr) {
      (void)tiling_data_.write(value, static_cast<std::streamsize>(size));
      (void)tiling_data_.flush();
    } else {
      auto addr = ::ge::ValueToPtr(::ge::PtrToValue(addr_base_) + offset_);
      if (memcpy_s(addr, static_cast<size_t>(max_size_ - offset_), value, size) != EOK) {
        GELOGE(ge::GRAPH_FAILED, "[Add][TilingData] Memcpy tiling data failed, "
               "dst size = %zu, src size = %zu.", static_cast<size_t>(max_size_ - offset_), size);
        REPORT_INNER_ERR_MSG("E19999", "[Add][TilingData] Memcpy tiling data failed, dst size = %zu, src size = %zu.",
                             static_cast<size_t>(max_size_ - offset_), size);
        return;
      }
      offset_ += size;
    }
  }

  void AlignOffsetWith64() {
    const uint64_t offset = (offset_ + sizeof(uint64_t) - 1U) / sizeof(uint64_t);
    offset_ = offset * sizeof(uint64_t);
  }
  void* GetAddrBase(uint64_t& max_size) const {
    max_size = max_size_;
    return addr_base_;
  }

  void SetAddrBaseOffset(const uint64_t size) {
    offset_ = size;
  }

  const ByteBuffer &GetAllTilingData() const { return tiling_data_; }

  ByteBuffer &GetAllTilingData() { return tiling_data_; }

  uint64_t GetTilingDataSize() const { return offset_; }
  void SetAllTilingData(const ByteBuffer &value) {
    tiling_data_.clear();
    offset_ = 0;
    AddTilingData(value.str().c_str(), value.str().size());
  }

  void SetClearAtomic(const bool clear_atomic) { clear_atomic_ = clear_atomic; }

  bool GetClearAtomic() const { return clear_atomic_; }

  void SetTilingKey(const uint64_t &tiling_key) { tiling_key_ = tiling_key; }

  uint64_t GetTilingKey() const { return tiling_key_; }

  void ResetWorkspace() {
    workspaces_.clear();
  }

  void ResetAddrBase(void *const addr_base, const uint64_t max_size) {
    addr_base_ = addr_base;
    max_size_ = max_size;
    offset_ = 0;
  }

  void SetTilingCond(const int32_t tiling_cond) { tiling_cond_ = tiling_cond; }

  int32_t GetTilingCond() const { return tiling_cond_; }
private:
  uint32_t block_dim_;
  bool clear_atomic_;
  uint64_t tiling_key_;
  ByteBuffer tiling_data_;
  std::vector<int64_t> workspaces_;
  void *addr_base_;
  uint64_t max_size_;
  uint64_t offset_;
  int32_t tiling_cond_;
};

  void OpRunInfo::AddTilingData(const ge::char_t *value, const size_t size) {
    (void)impl_->tiling_data_.write(value, static_cast<std::streamsize>(size));
    (void)impl_->tiling_data_.flush();
    return;
  }

  void OpRunInfo::SetBlockDim(const uint32_t &block_dim) {
    return;
  }

  void OpRunInfo::SetTilingKey(const uint64_t &new_tiling_key) {
    return;
  }

  const ByteBuffer &OpRunInfo::GetAllTilingData() const {
    return impl_->tiling_data_;
  }

  ByteBuffer &OpRunInfo::GetAllTilingData() {
    return impl_->tiling_data_;
  }

  OpRunInfo::OpRunInfo() {
    impl_ = make_shared<OpRunInfoImpl>();
  }
}
}
