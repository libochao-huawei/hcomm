#include "platform/platform_info.h"
#include "platform/platform_info_def.h"
#include "platform/platform_infos_def.h"
#include "llt_hccl_stub.h"

#include "exe_graph/runtime/tiling_context.h"
#include "exe_graph/runtime/tiling_parse_context.h"
namespace fe {
class PlatformInfoManager{
public:
    static PlatformInfoManager &Instance();
    uint32_t GetPlatformInfoWithOutSocVersion(PlatformInfo &platform_info, OptionalInfo &opti_compilation_info);
    uint32_t InitializePlatformInfo();
    uint32_t GetPlatformInfos(const std::string SoCVersion,
                            PlatFormInfos &platform_info,
                            OptionalInfos &opti_compilation_info);
}
}

namespace TbeReduce {
ge::graphStatus AutoTiling(gert::TilingContext* context);
ge::graphStatus AutoTilingParser(gert::TilingParseContext *context);
}