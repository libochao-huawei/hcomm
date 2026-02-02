#ifndef __HCCP_PUB_H__
#define __HCCP_PUB_H__
int HccpInit(unsigned int chipId, pid_t pid, int hdcType, unsigned int whiteListStatus);
int HccpDeinit(unsigned int chipId);
void RsGetCurTime(struct timeval *time);
void HccpTimeInterval(struct timeval *end_time, struct timeval *start_time, float *msec);
#endif
