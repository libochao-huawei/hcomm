#ifndef AIV_SK_AG_CROSSNODE_H
#define AIV_SK_AG_CROSSNODE_H
 
#include "aiv_communication_base.h"
#include "aiv_all_gather_crossnode_91093_graph.h"
// aiv reducescatter
 
extern "C" __aicore__ void sk_allgather_crossnode(SUPERKERNEL_LITE_ARGS_DEF) {
    SUPERKERNEL_LITE_ARGS_EXTRACT;
    return sk_all_gather_crossnode(SUPERKERNEL_ARGS_CALL);
}
 
 
#endif  /* AIV_AG_SUPERKERNEL_CROSSNODE_H */