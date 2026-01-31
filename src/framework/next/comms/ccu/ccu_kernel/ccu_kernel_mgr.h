/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu kernel manager header file
 * Create: 2025-02-13
 */

#ifndef HCOMM_CCU_KERNEL_MGR_H
#define HCOMM_CCU_KERNEL_MGR_H

#include <mutex>
#include <unordered_map>

#include "ccu_kernel.h"
#include "ccu_res_pack.h"

#include "ccu_dev_mgr_imp.h"
#include "../ccu_representation/reps/translator/ccu_rep_translator_v1.h"

namespace hcomm {

using namespace CcuRep;

class CcuKernelMgr {
public:
    static CcuKernelMgr &GetInstance(const s32 deviceLogicId);

    HcclResult Init();
    HcclResult Deinit();

    HcclResult Register(std::unique_ptr<CcuKernel> kernel,
        CcuResPack &resPack, CcuKernelHandle &kernelHandle);
    HcclResult UnRegister(const CcuKernelHandle kernelHandle);
    CcuKernel *GetKernel(const CcuKernelHandle kernelHandle);

private:
    explicit CcuKernelMgr() = default;
    ~CcuKernelMgr();

    CcuKernelMgr(const CcuKernelMgr &that) = delete;
    CcuKernelMgr &operator=(const CcuKernelMgr &that) = delete;

private:
    struct CcuTranslatResPack {
        std::vector<CcuResHandle> handles{};
    };

private:
    HcclResult AllocRes(std::unique_ptr<CcuKernel> &kernel, CcuResPack &resPack);
    HcclResult Translate(std::unique_ptr<CcuKernel> &kernel, bool isFuncBlock);

    HcclResult InstantiationTranslator(uint16_t dieId);
    HcclResult TransRepSequenceToMicrocode(std::unique_ptr<CcuKernel> &kernel,
        bool isFuncBlock);
    HcclResult LoadInstruction(const CcuRep::CcuInstrInfo &instrInfo, const uint32_t dieId);

    HcclResult GetResPackTotalResRepository(const CcuTranslatResPack &resPack,
        CcuResRepository &totalRes) const;

private:
    bool initializedFlag_{false};
    int32_t devLogicId_{-1};
    std::mutex kernelMapMutex_;
    CcuKernelHandle kernelId_ = 0;
    std::unordered_map<CcuKernelHandle, std::unique_ptr<CcuKernel>> kernelMap_{};
    void *instructionLoadDevMem_{nullptr};

    std::unordered_map<uint16_t, std::unordered_map<uint16_t, std::shared_ptr<CcuRepTranslator>>> translators;
    std::unordered_map<uint16_t, std::unordered_map<uint16_t, std::shared_ptr<CcuRepReferenceManager>>> referenceMgrs;
    CcuTranslatResPack translatorResPack{};
};
}; // namespace hcomm

#endif // HCOMM_CCU_KERNEL_MGR_IMP_H