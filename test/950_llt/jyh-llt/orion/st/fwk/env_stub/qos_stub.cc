#include "qos_stub.h"

QosErrorCode QosGetStreamEngineQos(
    QosStreamType label, QosEngineType engine, const std::string &op, int devId, QosConfig *info)
{
    return QosErrorCode::QOS_SUCCESS;
}
