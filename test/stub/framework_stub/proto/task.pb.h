/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GOOGLE_PROTOBUF_INCLUDED_task_2eproto
#define GOOGLE_PROTOBUF_INCLUDED_task_2eproto

namespace domi {

class KernelContext {
 public:
  uint32_t kernel_type() const;
};

class KernelDef {
 public:
    uint32_t num_blocks() const;
    const KernelContext& context() const;
};

class StarsSqeHeaderDef {
 public:
    uint32_t num_blocks() const;
};

class FftsPlusSqeDef {
 public:
  const StarsSqeHeaderDef& sqe_header() const;
};

class FftsPlusMixAicAivCtxDef {
 public:
  uint32_t non_tail_num_blocks() const;
  uint32_t non_tail_block_ratio_n() const;
};

class FftsPlusAicAivCtxDef {
 public:
  uint32_t non_tail_num_blocks() const;
};

class FftsPlusAicpuCtxDef {
 public:
  uint32_t non_tail_num_blocks() const;
};

class FftsPlusCtxDef {
 public:
  uint32_t context_type() const;
  uint32_t op_index() const;
  uint32_t context_id() const;
  const FftsPlusMixAicAivCtxDef& mix_aic_aiv_ctx() const;
  const FftsPlusAicAivCtxDef& aic_aiv_ctx() const;
  const FftsPlusAicpuCtxDef& aicpu_ctx() const;
};

class FftsPlusTaskDef {
 public:
  uint32_t op_index() const;
  const FftsPlusSqeDef& ffts_plus_sqe() const;
  int ffts_plus_ctx_size() const;
  const FftsPlusCtxDef& ffts_plus_ctx(int index) const;
};

class KernelHcclDef {
 public:
  KernelHcclDef();
  ~KernelHcclDef();
  uint32_t op_index() const;
};

class TaskDef {
 public:
  TaskDef();
  ~TaskDef();
  uint32_t type() const;
  int size() const;
  const KernelHcclDef& kernel_hccl() const;
  const KernelDef& kernel() const;
  const FftsPlusTaskDef& ffts_plus_task() const;
};

class ModelTaskDef {
 public:
  ModelTaskDef();
  ~ModelTaskDef();
  int task_size() const;
  const TaskDef& task(int index) const;
  const TaskDef& task() const;
};

class KernelDefWithHandle {
 public:
  const KernelContext& context() const;
};

class KernelExDef {
};

}

#endif
