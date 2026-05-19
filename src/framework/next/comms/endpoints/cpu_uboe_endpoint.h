#ifndef CPU_UBOE_ENDPOINT_H
#define CPU_UBOE_ENDPOINT_H

#include <cstdint>
#include "endpoint.h"

namespace hcomm {

/**
 * @note 当前阶段负责Host CPU通信引擎 + UBoE协议Endpoint控制面适配。
 *       多PORT ctx保存在Endpoint::ctxHandleList_中，内存管理临时复用UbRegedMemMgr并绑定PORT0 ctx，
 *       后续由UboeRegedMemMgr承接多PORT segment。
 */
class CpuUboeEndpoint : public Endpoint {
public:
    explicit CpuUboeEndpoint(const EndpointDesc &endpointDesc);

    ~CpuUboeEndpoint() override = default;

    HcclResult Init() override;

    HcclResult ServerSocketListen(const uint32_t port) override;
    HcclResult ServerSocketStopListen(const uint32_t port) override;

    HcclResult RegisterMemory(HcommMem mem, const char *memTag, void **memHandle) override;
    HcclResult UnregisterMemory(void *memHandle) override;
    HcclResult MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen) override;
    HcclResult MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem) override;
    HcclResult MemoryUnimport(const void *memDesc, uint32_t descLen) override;
    HcclResult GetAllMemHandles(void **memHandles, uint32_t *memHandleNum) override;

private:
    uint8_t GetPortNumFromEndpointDesc() const;
    HcclResult GetPortAddressByPortIdx(uint8_t idx, Hccl::IpAddress &ipAddr) const;
    HcclResult GetLinkAddress(Hccl::IpAddress &ipAddr) const;
};

} // namespace hcomm

#endif // CPU_UBOE_ENDPOINT_H
