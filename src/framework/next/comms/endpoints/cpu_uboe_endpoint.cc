#include <vector>

#include "endpoint_mgr.h"
#include "hccl_mem_defs.h"
#include "cpu_uboe_endpoint.h"
#include "hccl/hccl_res.h"
#include "log.h"
#include "reged_mems/urma_mem.h"
#include "adapter_rts_common.h"
#include "hccp_peer_manager.h"
#include "server_socket_manager.h"

using Hccl::HcclException;
using std::exception;
using std::string;

namespace hcomm {

CpuUboeEndpoint::CpuUboeEndpoint(const EndpointDesc &endpointDesc) : Endpoint(endpointDesc)
{
}

uint8_t CpuUboeEndpoint::GetPortNumFromEndpointDesc() const
{
    if (endpointDesc_.commAddr.type != COMM_ADDR_TYPE_MULTI_PORT) {
        return 1;
    }

    uint8_t portNum = endpointDesc_.commAddr.portsAddr.portNum;
    if (portNum == 0 || portNum > HCOMM_NIC_PORT_MAX_NUM) {
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u], max[%u].", __func__, portNum, HCOMM_NIC_PORT_MAX_NUM);
        return 0;
    }

    return portNum;
}

HcclResult CpuUboeEndpoint::GetPortAddressByPortIdx(uint8_t idx, Hccl::IpAddress &ipAddr) const
{
    if (endpointDesc_.commAddr.type != COMM_ADDR_TYPE_MULTI_PORT) {
        HCCL_ERROR("[CpuUboeEndpoint::%s] commAddr.type[%d] not support.", __func__, endpointDesc_.commAddr.type);
        return HCCL_E_NOT_SUPPORT;
    }

    uint8_t portNum = endpointDesc_.commAddr.portsAddr.portNum;
    CHK_PRT_RET(portNum == 0 || portNum > HCOMM_NIC_PORT_MAX_NUM,
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u].", __func__, portNum), HCCL_E_PARA);

    CHK_PRT_RET(idx >= portNum,
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portId[%u], portNum[%u].", __func__, idx, portNum), HCCL_E_PARA);

    Hccl::Eid inputEid;
    s32 sret
        = memcpy_s(inputEid.raw, Hccl::URMA_EID_LEN, endpointDesc_.commAddr.portsAddr.eidList[idx], COMM_ADDR_EID_LEN);
    CHK_PRT_RET(sret != EOK, HCCL_ERROR("[CpuUboeEndpoint::%s] memcpy eid failed, errorno[%d].", __func__, sret),
        HCCL_E_MEMORY);

    ipAddr = Hccl::IpAddress(inputEid);
    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::GetLinkAddress(Hccl::IpAddress &ipAddr) const
{
    if (endpointDesc_.commAddr.type != COMM_ADDR_TYPE_MULTI_PORT) {
        HCCL_ERROR("[CpuUboeEndpoint::%s] commAddr.type[%d] not support.", __func__, endpointDesc_.commAddr.type);
        return HCCL_E_NOT_SUPPORT;
    }

    Hccl::BinaryAddr addr;
    s32 sret = memcpy_s(
        &addr, sizeof(Hccl::BinaryAddr), &endpointDesc_.commAddr.portsAddr.linkAddr, sizeof(Hccl::BinaryAddr));
    CHK_PRT_RET(sret != EOK, HCCL_ERROR("[CpuUboeEndpoint::%s] memcpy linkAddr failed, errorno[%d].", __func__, sret),
        HCCL_E_MEMORY);

    ipAddr = Hccl::IpAddress(addr, endpointDesc_.commAddr.portsAddr.family);
    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::Init()
{
    HCCL_INFO("[CpuUboeEndpoint::%s] localEndpoint protocol[%d].", __func__, endpointDesc_.protocol);

    if (endpointDesc_.loc.locType != ENDPOINT_LOC_TYPE_HOST) {
        HCCL_ERROR("[CpuUboeEndpoint::%s] locType[%d] is not supported.", __func__, endpointDesc_.loc.locType);
        return HCCL_E_NOT_SUPPORT;
    }

    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));

    Hccl::HrtRaSocketSetWhiteListStatus(1);
    EXECEPTION_CATCH(Hccl::HccpPeerManager::GetInstance().Init(devId), return HCCL_E_INTERNAL);

    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(devId), devPhyId));

    portNum_ = GetPortNumFromEndpointDesc();
    CHK_PRT_RET(portNum_ == 0 || portNum_ > HCOMM_NIC_PORT_MAX_NUM,
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u].", __func__, portNum_), HCCL_E_PARA);

    ctxHandleList_.clear();

    auto &rdmaHandleMgr = Hccl::RdmaHandleManager::GetInstance();
    for (uint8_t portId = 0; portId < portNum_; ++portId) {
        Hccl::IpAddress ipAddr{};
        CHK_RET(GetPortAddressByPortIdx(portId, ipAddr));

        void *ctxHandle = nullptr;
        TRY_CATCH_RETURN(ctxHandle = static_cast<void *>(rdmaHandleMgr.GetByAddr(
                             devPhyId, Hccl::LinkProtoType::UB, ipAddr, Hccl::PortDeploymentType::HOST_NET)));
        CHK_PTR_NULL(ctxHandle);

        ctxHandleList_.push_back(ctxHandle);

        HCCL_INFO("[CpuUboeEndpoint::%s] init port success, portId[%u], devPhyId[%u], ipAddr[%s], ctxHandle[%p].",
            __func__, portId, devPhyId, ipAddr.Describe().c_str(), ctxHandle);
    }

    std::shared_ptr<UbRegedMemMgr> regedMemMgr = nullptr;
    EXECEPTION_CATCH(regedMemMgr = std::make_shared<UbRegedMemMgr>(), return HCCL_E_PARA);
    CHK_SMART_PTR_NULL(regedMemMgr);

    regedMemMgr->rdmaHandle_ = nullptr;
    regedMemMgr->rdmaHandleList_ = ctxHandleList_;
    regedMemMgr_ = regedMemMgr;

    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::ServerSocketListen(const uint32_t port)
{
    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));

    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(devId), devPhyId));

    CHK_PRT_RET(portNum_ == 0 || portNum_ > HCOMM_NIC_PORT_MAX_NUM,
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u].", __func__, portNum_), HCCL_E_PARA);

    Hccl::IpAddress ipAddr{};
    CHK_RET(GetLinkAddress(ipAddr));
    Hccl::PortData localPort
        = Hccl::PortData(devPhyId, Hccl::PortDeploymentType::HOST_NET, Hccl::LinkProtoType::UB, 0, ipAddr);
    HCCL_INFO("[CpuUboeEndpoint::%s] devPhyId[%u], port[%u], ipAddress[%s].", __func__, devPhyId, port,
        ipAddr.Describe().c_str());
    CHK_RET(ServerSocketManager::GetInstance().ServerSocketStartListen(
        localPort, Hccl::NicType::HOST_NIC_TYPE, devPhyId, port));

    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::ServerSocketStopListen(const uint32_t port)
{
    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));

    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(devId), devPhyId));

    CHK_PRT_RET(portNum_ == 0 || portNum_ > HCOMM_NIC_PORT_MAX_NUM,
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u].", __func__, portNum_), HCCL_E_PARA);

    Hccl::IpAddress ipAddr{};
    CHK_RET(GetLinkAddress(ipAddr));
    Hccl::PortData localPort
        = Hccl::PortData(devPhyId, Hccl::PortDeploymentType::HOST_NET, Hccl::LinkProtoType::UB, 0, ipAddr);
    HCCL_INFO("[CpuUboeEndpoint::%s] devPhyId[%u], port[%u], ipAddress[%s].", __func__, devPhyId, port,
        ipAddr.Describe().c_str());

    CHK_RET(ServerSocketManager::GetInstance().ServerSocketStopListen(localPort, Hccl::NicType::HOST_NIC_TYPE, port));
    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    CHK_SMART_PTR_NULL(regedMemMgr_);
    CHK_RET(regedMemMgr_->RegisterMemory(mem, memTag, memHandle));
    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::UnregisterMemory(void *memHandle)
{
    CHK_SMART_PTR_NULL(regedMemMgr_);
    CHK_RET(regedMemMgr_->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    CHK_SMART_PTR_NULL(regedMemMgr_);
    CHK_RET(regedMemMgr_->MemoryExport(endpointDesc_, memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    CHK_SMART_PTR_NULL(regedMemMgr_);
    CHK_RET(regedMemMgr_->MemoryImport(memDesc, descLen, outMem));
    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    CHK_SMART_PTR_NULL(regedMemMgr_);
    CHK_RET(regedMemMgr_->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    CHK_SMART_PTR_NULL(regedMemMgr_);
    CHK_RET(regedMemMgr_->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}

} // namespace hcomm
