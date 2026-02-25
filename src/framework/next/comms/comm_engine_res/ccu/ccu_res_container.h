/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_RES_CONTAINER_H
#define CCU_RES_CONTAINER_H

#include <memory>
#include <vector>

#include "hccl_res.h"

#include "ccu_kernel.h"
#include "ccu_res_pack.h"
#include "ccu_drv_handle.h"

namespace hcomm {

/**
 * @note 职责：管理通信域持有的CCU资源
 */
class CcuResContainer {
public:
    CcuResContainer(const uint32_t opExpansionMode) : opExpansionMode_(opExpansionMode) {};
    ~CcuResContainer();
    HcclResult Init();
    HcclResult ResetResPack();
    CcuResPack *GetResPack();
    HcclResult SaveCcuKernel(const CcuKernelHandle kernelHandle);
    const std::vector<CcuKernelHandle> &GetUntranslatedKernels();

private:
    uint32_t opExpansionMode_{0};
    int32_t devLogicId_{INT32_MAX};
    std::shared_ptr<hcomm::CcuDrvHandle> ccuDrvHandle_{};
    std::unique_ptr<CcuResPack> resPack_{};
    std::vector<CcuKernelHandle> kernelHandles_{};
    std::vector<CcuKernelHandle> untranslatedKernelHandles_{};
};

} // namespace hcomm
#endif // CCU_RES_CONTAINER_H