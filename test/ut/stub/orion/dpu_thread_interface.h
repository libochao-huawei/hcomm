#ifndef DPU_THREAD_INTERFACE_H
#define DPU_THREAD_INTERFACE_H

#include "hccl_types.h"

namespace Hccl {

class IDpuThread {
public:
    IDpuThread(){};
    ~IDpuThread(){};
    HcclResult GetRdmaHandle(void** rdmahandle) {return HCCL_SUCCESS;};
};
}  // namespace Hccl
#endif