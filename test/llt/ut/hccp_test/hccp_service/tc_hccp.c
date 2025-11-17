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
extern int hccp_add_to_cgroup();
extern int hccp_param_parse(int argc, char *argv[], struct hccp_init_param *param);
extern int hccp_set_log_info(struct hccp_init_param *param);
extern void rs_api_deinit(void);
extern int rs_api_init(void);
extern int hccp_change_num_of_file();
int dl_drv_get_dev_num(unsigned int *num_dev);
int dl_drv_device_get_phy_id_by_index(unsigned int devIndex, unsigned int *phyId);

void tc_normal()
{
	int argc = 4;
	char *argv[9] = {"llt_main", "--deviceId=2", "--pid=10",
		"--pidSign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a",
		"--logLevelInPid=0",
		"--hdcType=7",
		"--whiteListStatus=0",
        "--backupPhyId=0"};
	llt_main(argc, argv);
	mocker(dl_drv_get_dev_num, 10, 1);
	llt_main(argc, argv);
	mocker_clean();
	mocker(dl_drv_device_get_phy_id_by_index, 10, 1);
	llt_main(argc, argv);
	mocker_clean();

	mocker(dl_drv_get_dev_num, 10, 0);
	mocker(dl_drv_device_get_phy_id_by_index, 10, 0);
	struct hccp_init_param param = {0};
	param.chip_id = 1;
	hccp_param_parse(5, argv, &param);
	hccp_param_parse(6, argv, &param);
	hccp_param_parse(7, argv, &param);
	hccp_param_parse(8, argv, &param);
	mocker_clean();
	return;
}

void tc_abnormal()
{
    int argc = 3;
    char *argv[3] = {"llt_main", "--deviceId=6", "--pid=10"};
	llt_main(argc, argv);

	argc = 2;
	llt_main(argc, argv);

	argc = 3;
	char *argv_2[3] = {"llt_main", "--deviceId=6", "--pid=-10"};
	llt_main(argc, argv_2);

	argc = 4;
	char *argv_3[4] = {"llt_main", "--deviceId=", "--pid=", "--pid_sign="};
	llt_main(argc, argv_3);

	argc = 4;
	char *argv_4[4] = {"llt_main", "--devicerId=1", "--pid=", "--pid_sign="};
	llt_main(argc, argv_4);

	argc = 4;
	char *argv_5[4] = {"llt_main", "--deviceId=1a34", "--pid=123", "--pid_sign="};
	llt_main(argc, argv_5);

	argc = 4;
	char *argv_6[4] = {"llt_main", "=", "--pid=123", "--pid_sign="};
	llt_main(argc, argv_6);

	argc = 4;
	char *argv_10[4] = {"llt_main", "=", "--pid=123", "--pid_sign="};
	llt_main(argc, argv_10);

	argc = 4;
	char *argv_7[4] = {"llt_main", "--deviceId=6", "--pid10", "--pid_sign="};
	llt_main(argc, argv_7);

	argc = 4;
	char *argv_8[4] = {"llt_main", "--deviceId=6", "--pid10", "--pid_sign="};
	llt_main(argc, argv_8);

	argc = 4;
	char *argv_9[4] = {"llt_main", "--deviceId=6", "--pid10", "--pid_sign= "};
	llt_main(argc, argv_9);

	argc = 4;
	char *argv_13[4] = {"llt_main", "--deviceId=2", "--pid=-9", "--pid_sign= "};
	llt_main(argc, argv_13);

	argc = 4;
	char *argv_14[4] = {"llt_main", "--deviceId=2", "--pid=0", "--pid_sign= "};
	llt_main(argc, argv_14);

	mocker(sscanf_s, 1, -1);
	char *argv_15[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	llt_main(argc, argv_15);
	mocker_clean();

	argc = 4;
	char *argv_16[4] = {"llt_main", "--deviceId=2222222222222222222222222222222", "--pid=0", "--pid_sign= "};
	llt_main(argc, argv_16);

	mocker(snprintf_s, 1, -1);
	char *argv_17[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	llt_main(argc, argv_17);
	mocker_clean();

	mocker(dl_hal_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	char *argv_18[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	llt_main(argc, argv_18);
	mocker_clean();

	mocker(getpid, 10, -1);
	char *argv_19[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	llt_main(argc, argv_19);
	mocker_clean();

    argc = 4;
	mocker(dl_hal_init, 10, 0);
	mocker(hccp_param_parse, 10, 0);
	mocker(hccp_set_log_info, 10, 0);
	mocker(hccp_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	char *argv_20[4] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};
	llt_main(argc, argv_20);
	mocker_clean();

	mocker(hccp_change_num_of_file, 10, 1);
	llt_main(argc, argv_20);
	mocker_clean();

	mocker(dl_hal_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	mocker(hccp_param_parse, 10, 0);
	mocker(hccp_set_log_info, 10, 1);
	llt_main(argc, argv_20);
	mocker_clean();

	mocker(dl_hal_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	mocker(hccp_param_parse, 10, 0);
	mocker(hccp_set_log_info, 10, 0);
	mocker(hccp_init, 10, 1);
	llt_main(argc, argv_20);
	mocker_clean();

	mocker(hccp_param_parse, 10, 0);
	mocker(hccp_set_log_info, 10, 0);
	mocker(hccp_init, 10, 0);
	llt_main(argc, argv_20);
	mocker_clean();

	mocker(dl_hal_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	mocker(hccp_param_parse, 10, 0);
	mocker(hccp_set_log_info, 10, 0);
	mocker(hccp_init, 10, 0);
	mocker(SendStartUpFinishMsg, 10, -1);
	llt_main(argc, argv_20);
	mocker_clean();

	mocker(dl_hal_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	mocker(hccp_param_parse, 10, 0);
	mocker(hccp_set_log_info, 10, 0);
	mocker(rs_api_init, 10, 1);
	llt_main(argc, argv_20);
	mocker_clean();
    return;
}

