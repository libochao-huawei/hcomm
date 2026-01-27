/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : llt_hccl_stub_nic.h
  版 本 号   : 初稿
  作    者   : mali
  生成日期   : 2018年05月01日
  最近修改   :
  功能描述   : llt_hccl_stub_nic.cc 的私有头文件.
  函数列表   :
  修改历史   :
  1.日    期   : 2018年05月01日
    作    者   : mali
    修改内容   : 创建文件

******************************************************************************/

#ifndef __LLT_HCCL_STUB_PROFILING_PLUGIN_H__
#define __LLT_HCCL_STUB_PROFILING_PLUGIN_H__


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>         /* for assert  */
#include <errno.h>
#include <sys/time.h>       /* 获取时间 */

#include <hccl/base.h>
#include <hccl/hccl_types.h>
#include <securec.h>
#include <unistd.h>
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>       /* For mode constants */
#include <fcntl.h>          /* For O_* constants */

#include <string>
#include <list>
#include <map>
#include <iostream>
#include <fstream>
#include <mutex>

#include "llt_hccl_stub.h"
#include "task_profiling_pub.h"

#endif /* __LLT_HCCL_STUB_PROFILING_PLUGIN_H__ */

