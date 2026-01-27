#include "env_config_stub.h"

namespace Hccl {

EnvConfigStub::EnvConfigStub()
{
    Parse();
}

void EnvConfigStub::Parse()
{
    hostNicCfg.Parse();
    socketCfg.Parse();
    rtsCfg.Parse();
    rdmaCfg.Parse();
    algoCfg.Parse();
    logCfg.Parse();
    detourCfg.Parse();
}

const EnvHostNicConfig &EnvConfigStub::GetHostNicConfig()
{
    return hostNicCfg;
}

const EnvSocketConfig &EnvConfigStub::GetSocketConfig()
{
    return socketCfg;
}

const EnvRtsConfig &EnvConfigStub::GetRtsConfig()
{
    return rtsCfg;
}

const EnvRdmaConfig &EnvConfigStub::GetRdmaConfig()
{
    return rdmaCfg;
}

const EnvAlgoConfig &EnvConfigStub::GetAlgoConfig()
{
    return algoCfg;
}

const EnvLogConfig &EnvConfigStub::GetLogConfig()
{
    return logCfg;
}
using namespace std;
const EnvDetourConfig &EnvConfigStub::GetDetourConfig()
{
    return detourCfg;
}

} // namespace Hccl
