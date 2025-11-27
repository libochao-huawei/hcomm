#ifndef __HCCP_PUB_H__
#define __HCCP_PUB_H__
int hccp_init(unsigned int chip_id, pid_t pid, int hdc_type, unsigned int white_list_status);
int hccp_deinit(int dev_id);
void rs_get_cur_time(struct timeval *time);
void hccp_time_interval(struct timeval *end_time, struct timeval *start_time, float *msec);
#endif
