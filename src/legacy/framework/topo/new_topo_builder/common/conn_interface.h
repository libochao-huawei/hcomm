/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: ConnInterface header file
 * Create: 2024-12-30
 */

#ifndef CONN_INTERFACE_H
#define CONN_INTERFACE_H

#include <string>
#include "topo_common_types.h"
#include "ip_address.h"

namespace Hccl {

class ConnInterface {
public:
    // 使用地址信息、位置信息、链路类型、链路协议构造接口
    ConnInterface(const IpAddress inputAddr, const AddrPosition inputPos, const LinkType inputLinkType,
                  const LinkProtocol inputLinkProtocol);
    IpAddress    GetAddr() const;
    AddrPosition GetPos() const;
    LinkType     GetLinkType() const;
    LinkProtocol GetLinkProtocol() const;
    std::string  Describe() const;
    bool         operator==(const ConnInterface &rhs) const;
    bool         operator!=(const ConnInterface &rhs) const;

private:
    IpAddress    addr{};
    AddrPosition pos{};
    LinkType     linkType{};
    LinkProtocol linkProtocol{};
};
} // namespace Hccl

#endif // CONN_INTERFACE_H