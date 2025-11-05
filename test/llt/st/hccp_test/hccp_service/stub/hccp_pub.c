#include <stdlib.h>
#include <sys/time.h>

int hccp_init(unsigned int chip_id, pid_t pid, int hdc_type, unsigned int white_list_status)
{
	return 0;
}
int hccp_deinit(int dev_id)
{
	return 0;
}

#define MS_PER_SECOND_F   1000.0
#define MS_PER_SECOND_I   1000

void rs_get_cur_time(struct timeval *time)
{
    int ret;

    ret = gettimeofday(time, NULL);
    if (ret) {
        memset(time, 0, sizeof(struct timeval));
    }

    return;
}

void hccp_time_interval(struct timeval *end_time, struct timeval *start_time, float *msec)
{
    /* if low position is sufficient, then borrow one from the high position */
    if (end_time->tv_usec < start_time->tv_usec) {
        end_time->tv_sec -= 1;
        end_time->tv_usec += MS_PER_SECOND_I * MS_PER_SECOND_I;
    }

    *msec = (end_time->tv_sec - start_time->tv_sec) * MS_PER_SECOND_F +
            (end_time->tv_usec - start_time->tv_usec) / MS_PER_SECOND_F;

    return;
}

void rs_api_deinit(void)
{

}

int rs_api_init(void)
{
    return 0;
}
