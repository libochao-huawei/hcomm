#include "tsd.h"
#include "stdio.h"
#include <sys/types.h>
#include "param.h"
#include "dl_hal_function.h"

extern int llt_main(int argc, char* argv);
extern int sscanf_s(const char *buffer, const char *format, ...);
extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);
extern int hccp_add_to_cgroup();
extern void rs_api_deinit(void);
extern int rs_api_init(void);
extern int hccp_param_parse(int argc, char *argv[], struct hccp_init_param *param);
extern int hccp_set_log_info(struct hccp_init_param *param);
extern int hccp_change_num_of_file();
extern int hccp_init(unsigned int chip_id, pid_t pid, int hdc_type, unsigned int white_list_status);
int dl_drv_get_dev_num(unsigned int *num_dev);
int dl_drv_device_get_phy_id_by_index(unsigned int devIndex, unsigned int *phyId);
int dl_hal_init(void);

void tc_normal()
{
	int argc = 5;
	char *argv[9] = {"llt_main", "--deviceId=2", "--pid=10",
		"--pidSign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a",
		"--logLevelInPid=0",
		"--hdcType=7",
		"--whiteListStatus=0",
        "--backupPhyId=0"};
	mocker(dl_drv_get_dev_num, 10, 0);
	mocker(dl_drv_device_get_phy_id_by_index, 10, 0);
	mocker(dl_hal_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	fprintf(stderr, "argv[0] %s\n", argv[0]);
	fprintf(stderr, "argv[1] %s\n", argv[1]);
	fprintf(stderr, "argv[2] %s\n", argv[2]);
    fprintf(stderr, "argv[3] %s\n", argv[3]);
	llt_main(argc, argv);

	struct hccp_init_param param = {0};
	param.chip_id = 1;
	hccp_param_parse(argc, argv, &param);
	hccp_param_parse(6, argv, &param);
    hccp_param_parse(8, argv, &param);
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
        hccp_add_to_cgroup();

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
    
	mocker(hccp_change_num_of_file, 10, 1);
	llt_main(argc, argv_5);
	mocker_clean();

	mocker(dl_hal_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	mocker(hccp_param_parse, 10, 0);
	mocker(hccp_set_log_info, 10, 1);
	llt_main(argc, argv_5);
	mocker_clean();

	mocker(dl_hal_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	mocker(hccp_param_parse, 10, 0);
	mocker(hccp_set_log_info, 10, 0);
	mocker(rs_api_init, 10, 1);
	llt_main(argc, argv_5);
	mocker_clean();

	mocker(dl_hal_init, 10, 0);
	mocker(hccp_add_to_cgroup, 10, 0);
	mocker(hccp_param_parse, 10, 0);
	mocker(hccp_set_log_info, 10, 0);
	mocker(rs_api_init, 10, 0);
	mocker(hccp_init, 10, -1);
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

		mocker(dl_hal_init, 10, 0);
		mocker(hccp_add_to_cgroup, 10, 0);
		char *argv_18[5] = {"llt_main", "--deviceId=2", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a", "--logLevelInPid=0"};
		llt_main(argc, argv_18);
		mocker_clean();
                
                mocker(dl_hal_init, 10, 0);
                mocker(hccp_add_to_cgroup, 10, 0);
                char *argv_19[5] = {"llt_main", "--deviceId=1000", "--pid=10", "--pid_sign=ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a", "--logLevelInPid=0"};
                llt_main(argc, argv_19);
                mocker_clean();
               
                mocker(dl_hal_init, 10, 0);
                mocker(hccp_add_to_cgroup, 10, 0);
                mocker(hccp_param_parse, 10, 0);
                mocker(hccp_set_log_info, 10, 0);
                mocker(hccp_init, 10, 0);
                mocker(SendStartUpFinishMsg, 10, -1);
                llt_main(argc, argv_15);
                mocker_clean();
        return;
}

