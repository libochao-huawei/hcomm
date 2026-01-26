/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: PeerInfo header file
 * Create: 2024-12-16
 */
 
#ifndef NEW_PEER_INFO_H
#define NEW_PEER_INFO_H
 
#include "types.h"
#include "binary_stream.h"
#include "nlohmann/json.hpp"
 
namespace Hccl {

constexpr u32 MAX_PEER_LOCAL_ID = 64;
class PeerInfo {
public:
    PeerInfo() {};
    ~PeerInfo() {};
    u32         localId{0};
    void        Deserialize(const nlohmann::json &peerInfoJson);
    std::string Describe() const;
    explicit PeerInfo(BinaryStream& binaryStream);
    void GetBinStream(BinaryStream& binaryStream) const;
};
} // namespace Hccl

#endif // NEW_PEER_INFO_H