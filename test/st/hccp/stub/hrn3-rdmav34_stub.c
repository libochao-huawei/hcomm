#include <stddef.h>
#include <infiniband/verbs.h>

int roce_set_qp_lb_value(struct ibv_qp *qp, int lbValue)
{
    return 0;
}

int roce_get_qp_lb_value(struct ibv_qp *qp, int *lbValue)
{
    return 0;
}

int roce_get_qp_num(struct ibv_context *context, int *qpNum)
{
    return 0;
}