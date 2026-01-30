//
// Created by w00422550 on 10/21/23.
// Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
//
#include "virtual_topo.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include "types.h"
#include "const_val.h"
#include "dev_type.h"
#include "const_val.h"
#include "invalid_params_exception.h"
#include "not_support_exception.h"
#include "internal_exception.h"

namespace Hccl {

LinkData::LinkData(vector<char> &data)
{
    BinaryStream binaryStream(data);
    u32          val;
    binaryStream >> val;
    type = static_cast<PortDeploymentType::Value>(val);
    binaryStream >> val;
    linkProtocol_ = static_cast<LinkProtocol::Value>(val);
    binaryStream >> val;
    direction = static_cast<LinkDirection::Value>(val);
    binaryStream >> localRankId_;
    binaryStream >> remoteRankId_;
    binaryStream >> localPortId_;
    binaryStream >> remotePortId_;
    binaryStream >> readable;
    binaryStream >> writable;
    binaryStream >> hop;
    binaryStream >> localDieId_;

    u32 offset;
    u32 addrSize;
    binaryStream >> offset;
    binaryStream >> addrSize;

    std::vector<char> locAddr;
    if (data.begin() + offset >= data.end()) {
        THROW<InternalException>("[LinkData][LinkData]invalid offset[%u]", offset);
    }
    if (data.begin() + offset + addrSize >= data.end()) {
        THROW<InternalException>("[LinkData][LinkData]invalid addrSize[%u]", addrSize);
    }
    std::copy(data.begin() + offset, data.begin() + offset + addrSize, std::back_inserter(locAddr));
    std::vector<char> rmtAddr;
    std::copy(data.begin() + offset + addrSize, data.end(), std::back_inserter(rmtAddr));

    localAddr_  = IpAddress(locAddr);
    remoteAddr_ = IpAddress(rmtAddr);
}

std::vector<char> LinkData::GetUniqueId() const
{
    BinaryStream binaryStream;
    u32          val = static_cast<u32>(type);
    binaryStream << val;
    val = static_cast<u32>(linkProtocol_);
    binaryStream << val;
    val = static_cast<u32>(direction);
    binaryStream << val;
    binaryStream << localRankId_;
    binaryStream << remoteRankId_;
    binaryStream << localPortId_;
    binaryStream << remotePortId_;
    binaryStream << readable;
    binaryStream << writable;
    binaryStream << hop;
    binaryStream << localDieId_;

    vector<char> result;
    binaryStream.Dump(result);

    auto loc = localAddr_.GetUniqueId();
    auto rmt = remoteAddr_.GetUniqueId();

    u32 offset   = result.size();
    u32 addrSize = loc.size();

    offset += sizeof(offset) + sizeof(addrSize);

    binaryStream << offset;
    binaryStream << addrSize;
    binaryStream.Dump(result);
    result.insert(result.end(), loc.begin(), loc.end());
    result.insert(result.end(), rmt.begin(), rmt.end());
    return result;
}

} // namespace Hccl