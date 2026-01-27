#include "ascend_hal.h"

drvError_t halResourceIdCheck(struct drvResIdKey *info)
{
    return drvError_t::DRV_ERROR_NONE;
}

drvError_t halSqCqQuery(uint32_t devId, struct halSqCqQueryInfo *info)
{
    return drvError_t::DRV_ERROR_NONE;
}

drvError_t halSqCqConfig(uint32_t devId, struct halSqCqConfigInfo *info)
{
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halResourceIdInfoGet(struct drvResIdKey *key, drvResIdProcType type, uint64_t *value)
{
    return drvError_t::DRV_ERROR_NONE;
}
 
drvError_t halResAddrMap(unsigned int devId, struct res_addr_info *res_info, unsigned long *va, unsigned int *len)
{
    return drvError_t::DRV_ERROR_NONE;
}
 
drvError_t halResAddrUnmap(unsigned int devId, struct res_addr_info *res_info)
{
    return drvError_t::DRV_ERROR_NONE;
}

drvError_t halMemCtl(int type, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    return drvError_t::DRV_ERROR_NONE;
}

drvError_t halHostRegister(void *srcPtr, UINT64 size, UINT32 flag, UINT32 devid, void **dstPtr)
{
    return drvError_t::DRV_ERROR_NONE;
}

drvError_t halHostUnregister(void *srcPtr, UINT32 devid)
{
    return drvError_t::DRV_ERROR_NONE;
}

drvError_t halTsdrvCtl(uint32_t devId, int cmd, void *param, size_t paramSize, void *out, size_t *outSize)
{
    return drvError_t::DRV_ERROR_NONE;
}

drvError_t drvGetLocalDevIDByHostDevID(uint32_t host_dev_id, uint32_t *local_dev_id)
{
    return drvError_t::DRV_ERROR_NONE;
}

drvError_t drvMemcpy(DVdeviceptr dst, size_t destMax, DVdeviceptr src, size_t ByteCount)
{
    return drvError_t::DRV_ERROR_NONE;
}