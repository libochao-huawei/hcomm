#include "ccu_comp_pub.h"
#include "ccu_comp.h"
#include "hccl_common.h"
#include "log.h"

namespace hcomm {

HcclResult CcuSetTaskKill(const int32_t deviceLogicId)
{
    HCCL_INFO("[CcuSetTaskKill] Input params: deviceLogicId[%d]", deviceLogicId);
    // 入参校验拦截
    CHK_PRT_RET((deviceLogicId < 0 || static_cast<u32>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM),
        HCCL_ERROR("[CcuSetTaskKill]deviceLogicId[%d] error, MAX_MODULE_DEVICE_NUM[%u]", deviceLogicId, MAX_MODULE_DEVICE_NUM),
            HcclResult::HCCL_E_PARA);
    return CcuComponent::GetInstance(deviceLogicId).SetTaskKill();
}

HcclResult CcuSetTaskKillDone(const int32_t deviceLogicId)
{
    HCCL_INFO("[CcuSetTaskKillDone] Input params: deviceLogicId[%d]", deviceLogicId);
    // 入参校验拦截
    CHK_PRT_RET((deviceLogicId < 0 || static_cast<u32>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM),
        HCCL_ERROR("[CcuSetTaskKillDone]deviceLogicId[%d] error, MAX_MODULE_DEVICE_NUM[%u]", deviceLogicId, MAX_MODULE_DEVICE_NUM),
            HcclResult::HCCL_E_PARA);
    return CcuComponent::GetInstance(deviceLogicId).SetTaskKillDone();
}

HcclResult CcuCleanTaskKillState(const int32_t deviceLogicId)
{
    HCCL_INFO("[CcuCleanTaskKillState] Input params: deviceLogicId[%u]", deviceLogicId);
    // 入参校验拦截
    CHK_PRT_RET((deviceLogicId < 0 || static_cast<u32>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM),
        HCCL_ERROR("[CcuCleanTaskKillState]deviceLogicId[%d] error, MAX_MODULE_DEVICE_NUM[%u]", deviceLogicId, MAX_MODULE_DEVICE_NUM),
            HcclResult::HCCL_E_PARA);
    return CcuComponent::GetInstance(deviceLogicId).CleanTaskKillState();
}

HcclResult CcuCleanDieCkes(const int32_t deviceLogicId, const uint8_t dieId)
{
    HCCL_INFO("[CcuCleanDieCkes] Input params: deviceLogicId[%u], dieId[%u]", deviceLogicId, dieId);
    // 入参校验拦截
    CHK_PRT_RET((deviceLogicId < 0 || static_cast<u32>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM),
        HCCL_ERROR("[CcuCleanDieCkes]deviceLogicId[%d] error, MAX_MODULE_DEVICE_NUM[%u]", deviceLogicId, MAX_MODULE_DEVICE_NUM),
            HcclResult::HCCL_E_PARA);
    return CcuComponent::GetInstance(deviceLogicId).CleanDieCkes(dieId);
}

}
