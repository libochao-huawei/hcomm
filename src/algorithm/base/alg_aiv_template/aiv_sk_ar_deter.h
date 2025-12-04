#ifndef AIV_SK_AR_DETER_H
#define AIV_SK_AR_DETER_H
 
#include "aiv_communication_base.h"
#include "aiv_all_reduce_91093_deter.h"
// aiv reducescatter
 
extern "C" __aicore__ void sk_allreduce_deter(SUPERKERNEL_LITE_ARGS_DEF) {
    SUPERKERNEL_LITE_ARGS_EXTRACT;
    return sk_all_reduce_deter(SUPERKERNEL_ARGS_CALL);
}
 
 
#endif  /* AIV_AR_SUPERKERNEL_DETER_H */