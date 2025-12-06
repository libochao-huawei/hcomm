#ifndef AIV_SK_AG_CROSSNODE_H
#define AIV_SK_AG_CROSSNODE_H
 
#include "aiv_communication_base.h"
#include "aiv_all_to_all_91093_graph.h"
// aiv reducescatter
 
extern "C" __aicore__ void sk_alltoall_crossnode(SUPERKERNEL_LITE_ARGS_DEF) {
    SUPERKERNEL_LITE_ARGS_EXTRACT;
    return sk_all_to_all_crossnode(SUPERKERNEL_ARGS_CALL);
}
 
 
#endif  /* AIV_AG_SUPERKERNEL_CROSSNODE_H */