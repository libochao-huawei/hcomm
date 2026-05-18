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

namespace hcomm {

CpuUboeEndpoint::CpuUboeEndpoint(const EndpointDesc &endpointDesc)
    : Endpoint(endpointDesc)
{
}

uint8_t CpuUboeEndpoint::GetPortNumFromEndpointDesc() const
{
    if (endpointDesc_.commAddr.type != COMM_ADDR_TYPE_MULTI_PORT) {
        return 1;
    }

    uint8_t portNum = endpointDesc_.commAddr.portsAddr.portNum;
    if (portNum == 0 || portNum > HCOMM_NIC_PORT_MAX_NUM) {
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u], max[%u].",
            __func__, portNum, HCOMM_NIC_PORT_MAX_NUM);
        return 0;
    }

    return portNum;
}

HcclResult CpuUboeEndpoint::GetIpAddressByPortId(uint8_t portId, Hccl::IpAddress &ipAddr) const
{
    if (endpointDesc_.commAddr.type != COMM_ADDR_TYPE_MULTI_PORT) {
        CHK_PRT_RET(portId != 0,
            HCCL_ERROR("[CpuUboeEndpoint::%s] single address only supports portId 0, portId[%u].",
                __func__, portId),
            HCCL_E_PARA);

        CHK_RET(CommAddrToIpAddress(endpointDesc_.commAddr, ipAddr));
        return HCCL_SUCCESS;
    }

    uint8_t portNum = endpointDesc_.commAddr.portsAddr.portNum;
    CHK_PRT_RET(portNum == 0 || portNum > HCOMM_NIC_PORT_MAX_NUM,
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u].", __func__, portNum),
         HCCL_E_PARA);
 
    CHK_PRT_RET(portId >= portNum,
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portId[%u], portNum[%u].", __func__, portId, portNum),
        HCCL_E_PARA);

    Hccl::Eid inputEid;
    s32 sret = memcpy_s(inputEid.raw, Hccl::URMA_EID_LEN,
        endpointDesc_.commAddr.portsAddr.eidList[portId], COMM_ADDR_EID_LEN);
    CHK_PRT_RET(sret != EOK,
        HCCL_ERROR("[CpuUboeEndpoint::%s] memcpy eid failed, errorno[%d].", __func__, sret),
        HCCL_E_MEMORY);

    ipAddr = Hccl::IpAddress(inputEid);
    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::Init()
{
    HCCL_INFO("[CpuUboeEndpoint::%s] localEndpoint protocol[%d].", __func__, endpointDesc_.protocol);

    if (endpointDesc_.loc.locType != ENDPOINT_LOC_TYPE_HOST) {
        HCCL_ERROR("[CpuUboeEndpoint::%s] locType[%d] is not supported.",
            __func__, endpointDesc_.loc.locType);
        return HCCL_E_NOT_SUPPORT;
    }

    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));

    RaSocketSetWhiteListStatus(1);
    EXECEPTION_CATCH(Hccl::HccpPeerManager::GetInstance().Init(devId), return HCCL_E_INTERNAL);

    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(devId), devPhyId));

    portNum_ = GetPortNumFromEndpointDesc();
    CHK_PRT_RET(portNum_ == 0 || portNum_ > HCOMM_NIC_PORT_MAX_NUM,
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u].", __func__, portNum_),
        HCCL_E_PARA);

    ctxHandleList_.clear();

    auto &rdmaHandleMgr = Hccl::RdmaHandleManager::GetInstance();
    for (uint8_t portId = 0; portId < portNum_; ++portId) {
        Hccl::IpAddress ipAddr{};
        CHK_RET(GetIpAddressByPortId(portId, ipAddr));

        void *ctxHandle = nullptr;
        TRY_CATCH_RETURN(ctxHandle = static_cast<void *>(
            rdmaHandleMgr.GetByAddr(devPhyId, Hccl::LinkProtoType::UB, ipAddr, Hccl::PortDeploymentType::HOST_NET, Hccl::LinkProtocol::UBOE)));
        CHK_PTR_NULL(ctxHandle);

        ctxHandleList_.push_back(ctxHandle);

        HCCL_INFO("[CpuUboeEndpoint::%s] init port success, portId[%u], devPhyId[%u], ipAddr[%s], ctxHandle[%p].",
            __func__, portId, devPhyId, ipAddr.Describe().c_str(), ctxHandle);
    }

    ctxHandle_ = ctxHandleList_[0];

    std::shared_ptr<UbRegedMemMgr> regedMemMgr = nullptr;
    EXECEPTION_CATCH(regedMemMgr = std::make_shared<UbRegedMemMgr>(), return HCCL_E_PARA);
    CHK_SMART_PTR_NULL(regedMemMgr);

    // 临时方案：当前阶段仅完成CpuUboeEndpoint控制面适配。
    // 多PORT ctx已保存到ctxHandleList_，但内存管理暂时复用UbRegedMemMgr。
    // UbRegedMemMgr当前只能绑定一个rdmaHandle，因此默认绑定PORT0 ctxHandle_。
    // 后续新增UboeRegedMemMgr后，多PORT segment将下沉到RegedMemMgr内部处理。
    regedMemMgr->rdmaHandle_ = ctxHandle_;
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
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u].", __func__, portNum_),
        HCCL_E_PARA);

    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::UBOE);
    std::vector<Hccl::PortData> startedPorts;

    for (uint8_t portId = 0; portId < portNum_; ++portId) {
        Hccl::IpAddress ipAddr{};
        CHK_RET(GetIpAddressByPortId(portId, ipAddr));

        Hccl::PortData localPort = Hccl::PortData(devPhyId, type, 0, ipAddr);

        HcclResult ret = ServerSocketManager::GetInstance().ServerSocketStartListen(
            localPort, Hccl::NicType::HOST_NIC_TYPE, devPhyId, port);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[CpuUboeEndpoint::%s] start listen failed, portId[%u], ret[%d].",
                __func__, portId, ret);

            for (const auto &startedPort : startedPorts) {
                (void)ServerSocketManager::GetInstance().ServerSocketStopListen(
                    startedPort, Hccl::NicType::HOST_NIC_TYPE, port);
            }
            return ret;
        }

        startedPorts.push_back(localPort);
    }

    return HCCL_SUCCESS;
}

HcclResult CpuUboeEndpoint::ServerSocketStopListen(const uint32_t port)
{
    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));

    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(devId), devPhyId));

    CHK_PRT_RET(portNum_ == 0 || portNum_ > HCOMM_NIC_PORT_MAX_NUM,
        HCCL_ERROR("[CpuUboeEndpoint::%s] invalid portNum[%u].", __func__, portNum_),
        HCCL_E_PARA);

    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::UBOE);
    HcclResult firstRet = HCCL_SUCCESS;

    for (uint8_t portId = 0; portId < portNum_; ++portId) {
        Hccl::IpAddress ipAddr{};
        HcclResult ret = GetIpAddressByPortId(portId, ipAddr);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[CpuUboeEndpoint::%s] get ip address failed, portId[%u], ret[%d].",
                __func__, portId, ret);
            if (firstRet == HCCL_SUCCESS) {
                firstRet = ret;
            }
            continue;
        }

        Hccl::PortData localPort = Hccl::PortData(devPhyId, type, 0, ipAddr);

        HCCL_INFO("[CpuUboeEndpoint::%s] devPhyId[%u], portId[%u], ipAddress[%s], port[%u].",
            __func__, devPhyId, portId, ipAddr.Describe().c_str(), port);

        ret = ServerSocketManager::GetInstance().ServerSocketStopListen(
            localPort, Hccl::NicType::HOST_NIC_TYPE, port);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[CpuUboeEndpoint::%s] stop listen failed, portId[%u], ret[%d].",
                __func__, portId, ret);
            if (firstRet == HCCL_SUCCESS) {
                firstRet = ret;
            }
        }
    }

    return firstRet;
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
