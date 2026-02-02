#include "tsd.h"
#include "stdio.h"
#include <sys/types.h>
#include "param.h"
#include "dl_hal_function.h"

extern int llt_main(int argc, char* argv);
extern int sscanf_s(const char *buffer, const char *format, ...);
extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);
extern int HccpAddToCgroup();
extern void RsApiDeinit(void);
extern int RsApiInit(void);
extern int HccpParamParse(int argc, char *argv[], struct HccpInitParam *param);
extern int HccpSetLogInfo(struct HccpInitParam *param);
extern int HccpChangeNumOfFile();
extern int HccpInit(unsigned int chipId, pid_t pid, int hdcType, unsigned int whiteListStatus);
int dlDrvGetDevNum(unsigned int *numDev);
int dlDrvDeviceGetPhyIdByIndex(unsigned int devIndex, unsigned int *phyId);
int DlHalInit(void);

void tc_normal()
{
	int argc = 5;
	char *argv[9] = {"llt_main", "--deviceId=2", "--pid=10",
		"--pidSign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a",
		"--logLevelInPid=0",
		"--hdcType=7",
		"--whiteListStatus=0",
        "--backupPhyId=0"};
	mocker(DlDrvGetDevNum, 10, 0);
	mocker(DlDrvDeviceGetPhyIdByIndex, 10, 0);
	mocker(DlHalInit, 10, 0);
	mocker(HccpAddToCgroup, 10, 0);
	fprintf(stderr, "argv[0] %s\n", argv[0]);
	fprintf(stderr, "argv[1] %s\n", argv[1]);
	fprintf(stderr, "argv[2] %s\n", argv[2]);
    fprintf(stderr, "argv[3] %s\n", argv[3]);
	llt_main(argc, argv);

	struct HccpInitParam param = {0};
	param.chipId = 1;
	HccpParamParse(argc, argv, &param);
	HccpParamParse(6, argv, &param);
    HccpParamParse(8, argv, &param);
	mocker_clean();
	return;
}

void tc_abnormal()
{
        int argc = 3;
        char *argv[3] = {"llt_main", "--deviceID=6", "--pid=111"};

        fprintf(stderr, "argv[0] %s\n", argv[0]);
        fprintf(stderr, "argv[1] %s\n", argv[1]);
        fprintf(stderr, "argv[2] %s\n", argv[2]);
        llt_main(argc, argv);
        HccpAddToCgroup();

	argc = 3;
	llt_main(argc, argv);

	argc = 4;
	char *argv_2[3] = {"llt_main", "--deviceID=6", "--pid=-111"};
	llt_main(argc, argv_2);

	argc = 5;
	char *argv_3[3] = {"llt_main", "--deviceID=6", "--pid"};
	llt_main(argc, argv_3);

	argc = 5;
	char *argv_4[5] = {"llt_main", "--devicerId=1", "--pid=", "--pid_sign=", "--logLevelInPid=0"};
	llt_main(argc, argv_4);

	argc = 5;
	char *argv_5[5] = {"llt_main", "--deviceId=1a34", "--pid=123", "--pid_sign=", "--logLevelInPid=0"};
	llt_main(argc, argv_5);
    
	mocker(HccpChangeNumOfFile, 10, 1);
	llt_main(argc, argv_5);
	mocker_clean();

	mocker(DlHalInit, 10, 0);
	mocker(HccpAddToCgroup, 10, 0);
	mocker(HccpParamParse, 10, 0);
	mocker(HccpSetLogInfo, 10, 1);
	llt_main(argc, argv_5);
	mocker_clean();

	mocker(DlHalInit, 10, 0);
	mocker(HccpAddToCgroup, 10, 0);
	mocker(HccpParamParse, 10, 0);
	mocker(HccpSetLogInfo, 10, 0);
	mocker(RsApiInit, 10, 1);
	llt_main(argc, argv_5);
	mocker_clean();

	mocker(DlHalInit, 10, 0);
	mocker(HccpAddToCgroup, 10, 0);
	mocker(HccpParamParse, 10, 0);
	mocker(HccpSetLogInfo, 10, 0);
	mocker(RsApiInit, 10, 0);
	mocker(HccpInit, 10, -1);
	llt_main(argc, argv_5);
	mocker_clean();

	argc = 5;
	char *argv_6[4] = {"llt_main", "=", "--pid=123", "--pid_sign="};
	llt_main(argc, argv_6);

	argc = 5;
	char *argv_10[4] = {"llt_main", "=", "--pid=123", "--pid_sign="};
	llt_main(argc, argv_10);

        argc = 5;
        char *argv_7[5] = {"llt_main", "--deviceId=6", "--pid10", "--pid_sign=", "--logLevelInPid=0"};
        llt_main(argc, argv_7);

        argc = 5;
        char *argv_8[5] = {"llt_main", "--deviceId=6", "--pid=10", "--pid_sign=", "--logLevelInPid=0"};
        llt_main(argc, argv_8);

        argc = 5;
        char *argv_9[5] = {"llt_main", "--deviceId=6", "--pid10", "--pid_sign= ", "--logLevelInPid=0"};
        llt_main(argc, argv_9);

		argc = 5;
        char *argv_11[5] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign= ", "--logLevelInPid=0"};
        llt_main(argc, argv_11);

		argc = 5;
        char *argv_12[5] = {"llt_main", "--deviceId=2", "--pid10", "--pid_sign= ", "--logLevelInPid=0"};
        llt_main(argc, argv_12);

		argc = 5;
        char *argv_13[5] = {"llt_main", "--deviceId=2", "--pid=-9", "--pid_sign= ", "--logLevelInPid=0"};
        llt_main(argc, argv_13);

		argc = 5;
        char *argv_14[5] = {"llt_main", "--deviceId=", "--pid=-9", "--pid_sign= ", "--logLevelInPid=0"};
        llt_main(argc, argv_14);

		mocker(sscanf_s, 1, -1);
		char *argv_15[5] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a", "--logLevelInPid=0"};
		llt_main(argc, argv_15);
		mocker_clean();

		mocker(snprintf_s, 1, -1);
		char *argv_17[5] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a", "--logLevelInPid=0"};
		llt_main(argc, argv_17);
		mocker_clean();

		mocker(DlHalInit, 10, 0);
		mocker(HccpAddToCgroup, 10, 0);
		char *argv_18[5] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a", "--logLevelInPid=0"};
		llt_main(argc, argv_18);
		mocker_clean();
                
                mocker(DlHalInit, 10, 0);
                mocker(HccpAddToCgroup, 10, 0);
                char *argv_19[5] = {"llt_main", "--deviceId=1000", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a", "--logLevelInPid=0"};
                llt_main(argc, argv_19);
                mocker_clean();
               
                mocker(DlHalInit, 10, 0);
                mocker(HccpAddToCgroup, 10, 0);
                mocker(HccpParamParse, 10, 0);
                mocker(HccpSetLogInfo, 10, 0);
                mocker(HccpInit, 10, 0);
                mocker(SendStartUpFinishMsg, 10, -1);
                llt_main(argc, argv_15);
                mocker_clean();
        return;
}

