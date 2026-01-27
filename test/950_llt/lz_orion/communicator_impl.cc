HcclResult CommunicatorImpl::GetCacheMap(AivOpCacheArgs& opCacheParam , std::shared_ptr<InsQueue>& tempInsQue)
{
    if (hcclCacheMap_.size() > CACHEMAP_MAXSIZE) {
        size_t clearCount = CACHEMAP_MAXSIZE * CACHEMAP_CLEARPERCENT;
        for (auto it = hcclCacheMap_.begin(); clearCount > 0 && it != hcclCacheMap_.end(); clearCount--) {
            it = hcclCacheMap_.erase(it);
        }
    }
    hcclCacheMap_.emplace(std::make_pair(opCacheParam, std::move(tempInsQue)));
    HCCL_INFO("[CommunicatorImpl][GetCacheMap]");
    return HCCL_SUCCESS;
}

