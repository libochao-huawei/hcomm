/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: PeerInfo file
 * Create: 2024-12-16
 */

#include "peer_info.h"
#include "json_parser.h"
#include "invalid_params_exception.h"

namespace Hccl {

void PeerInfo::Deserialize(const nlohmann::json &peerInfoJson)
{
    std::string msgId = "[PeerInfo::Deserialize] error occurs when parser object of propName \"local_id\"";
    TRY_CATCH_THROW(InvalidParamsException, msgId, localId = GetJsonPropertyUInt(peerInfoJson, "local_id"););

    if (localId > MAX_PEER_LOCAL_ID) {
        THROW<InvalidParamsException>("[PeerInfo::%s] localId[%u] is out of range [0, %u].", __func__, localId, MAX_PEER_LOCAL_ID);
    }
}

std::string PeerInfo::Describe() const
{
    return StringFormat("PeerInfo{local_id=%u}", localId);
}

void PeerInfo::GetBinStream(BinaryStream &binaryStream) const
{
    binaryStream << localId;
}

PeerInfo::PeerInfo(BinaryStream &binaryStream)
{
    binaryStream >> localId;
}

} // namespace Hccl