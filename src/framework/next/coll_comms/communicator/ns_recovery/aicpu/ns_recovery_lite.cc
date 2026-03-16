#include "ns_recovery_lite.h"

namespace hccl
{

NsRecoveryLite::NsRecoveryLite(const HDCommunicatePtr& kfcControlTransferH2D, const HDCommunicatePtr& kfcStatusTransferD2H)
{
    hdcHandler_ = std::make_unique<AicpuHdcHandler>(kfcControlTransferH2D, kfcStatusTransferD2H);
}

Hccl::KfcCommand NsRecoveryLite::BackGroundGetCmd()
{
    std::unique_lock<std::mutex> lock(hdcShmLock_);
    return hdcHandler_->GetKfcCommand();
}

void NsRecoveryLite::BackGroundSetStatus(Hccl::KfcStatus status, Hccl::KfcErrType errorCode)
{
    std::unique_lock<std::mutex> lock(hdcShmLock_);
    hdcHandler_->SetKfcExecStatus(status, errorCode);
}

void NsRecoveryLite::ResetErrorReported() 
{
    isErrorReported_ = false;
}
void NsRecoveryLite::SetNeedClean(bool flag)
{
    needClean_ = flag;
}
bool NsRecoveryLite::IsNeedClean() const
{
    return needClean_;
}
void NsRecoveryLite::SetIsSuspended(bool status)
{
    isSuspended_ = status;
}
bool NsRecoveryLite::IsSuspended() const
{
    return isSuspended_;
}

}