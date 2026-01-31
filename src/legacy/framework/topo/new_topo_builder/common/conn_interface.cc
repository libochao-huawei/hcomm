/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: ConnInterface
 * Create: 2024-12-30
 */

#include "conn_interface.h"
#include "string_util.h"

namespace Hccl {

ConnInterface::ConnInterface(const IpAddress inputAddr, const AddrPosition inputPos, const LinkType inputLinkType,
                             const LinkProtocol inputLinkProtocol)
    : addr(inputAddr), pos(inputPos), linkType(inputLinkType), linkProtocol(inputLinkProtocol)
{
}

IpAddress ConnInterface::GetAddr() const
{
    return addr;
}

AddrPosition ConnInterface::GetPos() const
{
    return pos;
}

LinkType ConnInterface::GetLinkType() const
{
    return linkType;
}

LinkProtocol ConnInterface::GetLinkProtocol() const
{
    return linkProtocol;
}

std::string ConnInterface::Describe() const
{
    return StringFormat("ConnInterface[addr=%s, pos=%s]", addr.Describe().c_str(), pos.Describe().c_str());
}

bool ConnInterface::operator==(const ConnInterface &rhs) const
{
    return addr == rhs.addr && pos == rhs.pos && linkType == rhs.linkType && linkProtocol == rhs.linkProtocol;
}

bool ConnInterface::operator!=(const ConnInterface &rhs) const
{
    return !(rhs == *this);
}
} // namespace Hccl