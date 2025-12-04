#ifndef AIV_SK_RS_CROSSNODE_H
#define AIV_SK_RS_CROSSNODE_H
 
#include "aiv_communication_base.h"
#include "aiv_reduce_scatter_crossnode_91093_graph.h"
// aiv reducescatter
 
extern "C" __aicore__ void sk_reducescatter_crossnode(SUPERKERNEL_LITE_ARGS_DEF) {
    SUPERKERNEL_LITE_ARGS_EXTRACT;
    return sk_reduce_scatter_crossnode(SUPERKERNEL_ARGS_CALL);
}
 
 
#endif  /* AIV_RS_SUPERKERNEL_CROSSNODE_H */