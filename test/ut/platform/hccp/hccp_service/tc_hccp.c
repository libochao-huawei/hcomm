/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include "tsd.h"
#include "stdio.h"
#include "ut_dispatch.h"
#include "hccp_pub.h"
#include "securec.h"
#include "param.h"
#include "dl_hal_function.h"

extern int llt_main(int argc, char* argv);
extern int HccpAddToCgroup();
extern int HccpParamParse(int argc, char *argv[], struct HccpInitParam *param);
extern int HccpSetLogInfo(struct HccpInitParam *param);
extern void RsApiDeinit(void);
extern int RsApiInit(void);
extern int HccpChangeNumOfFile();
int dlDrvGetDevNum(unsigned int *numDev);
int dlDrvDeviceGetPhyIdByIndex(unsigned int devIndex, unsigned int *phyId);

void tc_normal()
{
	// int argc = 4;
	// char *argv[9] = {"llt_main", "--deviceId=2", "--pid=10",
	// 	"--pidSign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a",
	// 	"--logLevelInPid=0",
	// 	"--hdcType=7",
	// 	"--whiteListStatus=0",
    //     "--backupPhyId=0"};
	// llt_main(argc, argv);
	// mocker(DlDrvGetDevNum, 10, 1);
	// llt_main(argc, argv);
	// mocker_clean();
	// mocker(DlDrvDeviceGetPhyIdByIndex, 10, 1);
	// llt_main(argc, argv);
	// mocker_clean();

	// mocker(DlDrvGetDevNum, 10, 0);
	// mocker(DlDrvDeviceGetPhyIdByIndex, 10, 0);
	// struct HccpInitParam param = {0};
	// param.chipId = 1;
	// HccpParamParse(5, argv, &param);
	// HccpParamParse(6, argv, &param);
	// HccpParamParse(7, argv, &param);
	// HccpParamParse(8, argv, &param);
	// mocker_clean();
	return;
}

void tc_abnormal()
{
    // int argc = 3;
    // char *argv[3] = {"llt_main", "--deviceId=6", "--pid=10"};
	// llt_main(argc, argv);

	// argc = 2;
	// llt_main(argc, argv);

	// argc = 3;
	// char *argv_2[3] = {"llt_main", "--deviceId=6", "--pid=-10"};
	// llt_main(argc, argv_2);

	// argc = 4;
	// char *argv_3[4] = {"llt_main", "--deviceId=", "--pid=", "--pid_sign="};
	// llt_main(argc, argv_3);

	// argc = 4;
	// char *argv_4[4] = {"llt_main", "--devicerId=1", "--pid=", "--pid_sign="};
	// llt_main(argc, argv_4);

	// argc = 4;
	// char *argv_5[4] = {"llt_main", "--deviceId=1a34", "--pid=123", "--pid_sign="};
	// llt_main(argc, argv_5);

	// argc = 4;
	// char *argv_6[4] = {"llt_main", "=", "--pid=123", "--pid_sign="};
	// llt_main(argc, argv_6);

	// argc = 4;
	// char *argv_10[4] = {"llt_main", "=", "--pid=123", "--pid_sign="};
	// llt_main(argc, argv_10);

	// argc = 4;
	// char *argv_7[4] = {"llt_main", "--deviceId=6", "--pid10", "--pid_sign="};
	// llt_main(argc, argv_7);

	// argc = 4;
	// char *argv_8[4] = {"llt_main", "--deviceId=6", "--pid10", "--pid_sign="};
	// llt_main(argc, argv_8);

	// argc = 4;
	// char *argv_9[4] = {"llt_main", "--deviceId=6", "--pid10", "--pid_sign= "};
	// llt_main(argc, argv_9);

	// argc = 4;
	// char *argv_13[4] = {"llt_main", "--deviceId=2", "--pid=-9", "--pid_sign= "};
	// llt_main(argc, argv_13);

	// argc = 4;
	// char *argv_14[4] = {"llt_main", "--deviceId=2", "--pid=0", "--pid_sign= "};
	// llt_main(argc, argv_14);

	// mocker(sscanf_s, 1, -1);
	// char *argv_15[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	// llt_main(argc, argv_15);
	// mocker_clean();

	// argc = 4;
	// char *argv_16[4] = {"llt_main", "--deviceId=2222222222222222222222222222222", "--pid=0", "--pid_sign= "};
	// llt_main(argc, argv_16);

	// mocker(snprintf_s, 1, -1);
	// char *argv_17[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	// llt_main(argc, argv_17);
	// mocker_clean();

	// mocker(DlHalInit, 10, 0);
	// mocker(HccpAddToCgroup, 10, 0);
	// char *argv_18[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	// llt_main(argc, argv_18);
	// mocker_clean();

	// mocker(getpid, 10, -1);
	// char *argv_19[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	// llt_main(argc, argv_19);
	// mocker_clean();

    // argc = 4;
	// mocker(DlHalInit, 10, 0);
	// mocker(HccpParamParse, 10, 0);
	// mocker(HccpSetLogInfo, 10, 0);
	// mocker(HccpInit, 10, 0);
	// mocker(HccpAddToCgroup, 10, 0);
	// char *argv_20[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	// llt_main(argc, argv_20);
	// mocker_clean();

	// mocker(HccpChangeNumOfFile, 10, 1);
	// llt_main(argc, argv_20);
	// mocker_clean();

	// mocker(DlHalInit, 10, 0);
	// mocker(HccpAddToCgroup, 10, 0);
	// mocker(HccpParamParse, 10, 0);
	// mocker(HccpSetLogInfo, 10, 1);
	// llt_main(argc, argv_20);
	// mocker_clean();

	// mocker(DlHalInit, 10, 0);
	// mocker(HccpAddToCgroup, 10, 0);
	// mocker(HccpParamParse, 10, 0);
	// mocker(HccpSetLogInfo, 10, 0);
	// mocker(HccpInit, 10, 1);
	// llt_main(argc, argv_20);
	// mocker_clean();

	// mocker(HccpParamParse, 10, 0);
	// mocker(HccpSetLogInfo, 10, 0);
	// mocker(HccpInit, 10, 0);
	// llt_main(argc, argv_20);
	// mocker_clean();

	// mocker(DlHalInit, 10, 0);
	// mocker(HccpAddToCgroup, 10, 0);
	// mocker(HccpParamParse, 10, 0);
	// mocker(HccpSetLogInfo, 10, 0);
	// mocker(HccpInit, 10, 0);
	// mocker(SendStartUpFinishMsg, 10, -1);
	// llt_main(argc, argv_20);
	// mocker_clean();

	// mocker(DlHalInit, 10, 0);
	// mocker(HccpAddToCgroup, 10, 0);
	// mocker(HccpParamParse, 10, 0);
	// mocker(HccpSetLogInfo, 10, 0);
	// mocker(RsApiInit, 10, 1);
	// llt_main(argc, argv_20);
	// mocker_clean();
    return;
}

