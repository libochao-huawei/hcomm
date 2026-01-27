    
#include "hccl_aiv_utils.h"
std::map<AivOpCacheArgs, std::shared_ptr<InsQueue>> hcclCacheMap_; //存储aiv cache信息
HcclResult GetCacheMap(AivOpCacheArgs& opCacheParam , std::shared_ptr<InsQueue>& tempInsQue);