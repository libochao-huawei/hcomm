/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: exchange rdma connection dto header file
 * Create: 2025-11-27
 */
#ifndef HCCLV2_EXCHANGE_RDMA_CONN_DTO_H
#define HCCLV2_EXCHANGE_RDMA_CONN_DTO_H

#include <string>
#include "binary_stream.h"
#include "serializable.h"
#include "orion_adapter_hccp.h"
namespace Hccl {
class ExchangeRdmaConnDto : public Serializable {
public:
    ExchangeRdmaConnDto(){};

    ExchangeRdmaConnDto(uint32_t qpn, uint32_t psn, uint32_t gid_idx) : qpn_(qpn), psn_(psn), gid_idx_(gid_idx){};

    void Serialize(Hccl::BinaryStream &stream) override
    {
        stream << qpn_ << psn_ << gid_idx_ << gid_;
    }

    void Deserialize(Hccl::BinaryStream &stream) override
    {
        stream >> qpn_ >> psn_ >> gid_idx_ >> gid_;
    }

    std::string Describe() const override
    {
        return StringFormat("ExchangeRdmaConnDto=[qpn=%u, psn=%u, gid_idx=%u]",
                            qpn_, psn_, gid_idx_);
    }

// RaTypicalQpModify
    uint32_t qpn_;
    uint32_t psn_;
    uint32_t gid_idx_;
    uint8_t gid_[HCCP_GID_RAW_LEN];
};

} // namespace Hccl

#endif // HCCLV2_EXCHANGE_RDMA_CONN_DTO_H