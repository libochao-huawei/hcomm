#include <stdlib.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <errno.h>
#include "hccp_ping.h"
#include "ra_rs_comm.h"
#include "ra_comm.h"

int rs_ping_handle_init(unsigned int chip_id, int hdc_type, unsigned int white_list_status)
{
    return 0;
}

int rs_ping_handle_deinit(unsigned int chip_id)
{
    return 0;
}

int rs_ping_init(struct ping_init_attr *attr, struct ping_init_info *info, unsigned int *rdev_index)
{
    return 0;
}

int rs_ping_target_add(struct ra_rs_dev_info *rdev, struct ping_target_info *target)
{
    return 0;
}

int rs_ping_task_start(struct ra_rs_dev_info *rdev, struct ping_task_attr *attr)
{
    return 0;
}

int rs_ping_get_results(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[], unsigned int *num,
    struct ping_result_info result[])
{
    return 0;
}

int rs_ping_task_stop(struct ra_rs_dev_info *rdev)
{
    return 0;
}

int rs_ping_target_del(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[], unsigned int *num)
{
    return 0;
}

int rs_ping_deinit(struct ra_rs_dev_info *rdev)
{
    return 0;
}

