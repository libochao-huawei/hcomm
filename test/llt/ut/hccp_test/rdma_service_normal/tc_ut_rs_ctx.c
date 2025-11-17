#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "ut_dispatch.h"
#include "rs_inner.h"
#include "rs_ub.h"
#include "rs_ctx.h"
#include "rs_ccu.h"
#include "rs_esched.h"
#include "tc_ut_rs_ctx.h"
#include "ascend_hal_dl.h"
#include "dl_hal_function.h"
#include "hccp_msg.h"

extern void rs_ub_ctx_drv_jetty_delete(struct rs_ctx_jetty_cb *jetty_cb);
extern void rs_ub_ctx_free_jetty_cb(struct rs_ctx_jetty_cb *jetty_cb);

int stub_rs_get_rs_cb(unsigned int phy_id, struct rs_cb **rs_cb)
{
    static struct rs_cb rs_cb_tmp = {0};

    rs_cb_tmp.protocol = 1; //PROTOCOL_UDMA
    *rs_cb = &rs_cb_tmp;
    return 0;
}

void tc_rs_get_dev_eid_info_num()
{
    unsigned int num = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_eid_info_num, 1, 0);
    ret = rs_get_dev_eid_info_num(0, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_get_dev_eid_info_list()
{
    struct dev_eid_info info_list[5] = {0};
    unsigned int num = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_eid_info_list, 1, 0);
    ret = rs_get_dev_eid_info_list(0, info_list, 0, &num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_init()
{
    struct dev_base_attr dev_attr = {0};
    struct ctx_init_attr attr = {0};
    unsigned int dev_index = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_setup_sharemem, 1, 0);
    mocker(rs_ub_ctx_init, 1, 0);
    ret = rs_ctx_init(&attr, &dev_index, &dev_attr);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_deinit()
{
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_deinit, 1, 0);
    ret = rs_ctx_deinit(&dev_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_token_id_alloc()
{
    struct ra_rs_dev_info dev_info = {0};
    unsigned long long addr = 0;
    unsigned int token_id = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_token_id_alloc, 1, 0);
    ret = rs_ctx_token_id_alloc(&dev_info, &addr, &token_id);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_token_id_free()
{
    struct ra_rs_dev_info dev_info = {0};
    unsigned long long addr = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_token_id_free, 1, 0);
    ret = rs_ctx_token_id_free(&dev_info, addr);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_lmem_reg()
{
    struct ra_rs_dev_info dev_info = {0};
    struct mem_reg_attr_t mem_attr = {0};
    struct mem_reg_info_t mem_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_lmem_reg, 1, 0);
    ret = rs_ctx_lmem_reg(&dev_info, &mem_attr, &mem_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_lmem_unreg()
{
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_lmem_unreg, 1, 0);
    ret = rs_ctx_lmem_unreg(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_rmem_import()
{
    struct ra_rs_dev_info dev_info = {0};
    struct mem_reg_attr_t mem_attr = {0};
    struct mem_reg_info_t mem_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_rmem_import, 1, 0);
    ret = rs_ctx_rmem_import(&dev_info, &mem_attr, &mem_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_rmem_unimport()
{
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_rmem_unimport, 1, 0);
    ret = rs_ctx_rmem_unimport(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_chan_create()
{
    struct ra_rs_dev_info dev_info = {0};
    unsigned long long addr = 0;
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_chan_create, 1, 0);
    ret = rs_ctx_chan_create(&dev_info, &addr);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_chan_destroy()
{
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_chan_destroy, 1, 0);
    ret = rs_ctx_chan_destroy(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_cq_create()
{
    struct ra_rs_dev_info dev_info = {0};
    struct ctx_cq_attr attr = {0};
    struct ctx_cq_info info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jfc_create, 1, 0);
    ret = rs_ctx_cq_create(&dev_info, &attr, &info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_cq_destroy()
{
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jfc_destroy, 1, 0);
    ret = rs_ctx_cq_destroy(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_create()
{
    struct ra_rs_dev_info dev_info = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_qp_attr qp_attr = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_create, 1, 0);
    ret = rs_ctx_qp_create(&dev_info, &qp_attr, &qp_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_destroy()
{
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_destroy, 1, 0);
    ret = rs_ctx_qp_destroy(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_import()
{
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_info = {0};
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_import, 1, 0);
    ret = rs_ctx_qp_import(&dev_info, &import_attr, &import_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_unimport()
{
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_unimport, 1, 0);
    ret = rs_ctx_qp_unimport(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_bind()
{
    struct rs_ctx_qp_info remote_qp_info = {0};
    struct rs_ctx_qp_info local_qp_info = {0};
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_bind, 1, 0);
    ret = rs_ctx_qp_bind(&dev_info, &local_qp_info, &remote_qp_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_qp_unbind()
{
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_unbind, 1, 0);
    ret = rs_ctx_qp_unbind(&dev_info, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_batch_send_wr()
{
    struct wrlist_send_complete_num wrlist_num = {0};
    struct wrlist_base_info base_info = {0};
    struct batch_send_wr_data wr_data = {0};
    struct send_wr_resp wr_resp = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_ctx_batch_send_wr, 1, 0);
    ret = rs_ctx_batch_send_wr(&base_info, &wr_data, &wr_resp, &wrlist_num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_update_ci()
{
    struct ra_rs_dev_info dev_info = {0};
    int ret = 0;

    mocker_clean();
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb, 1);
    mocker(rs_ub_get_dev_cb, 1, 0);
    mocker(rs_ub_ctx_jetty_update_ci, 1, 0);
    ret = rs_ctx_update_ci(&dev_info, 0, 0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ctx_custom_channel()
{
    struct custom_chan_info_out out = {0};
    struct custom_chan_info_in in = {0};
    int ret = 0;

    mocker_clean();
    mocker(rs_ctx_ccu_custom_channel, 1, 0);
    ret = rs_ctx_custom_channel(&in, &out);
    mocker_clean();
}

void tc_rs_ctx_esched()
{
    ts_ccu_task_report_t ccu_task_info = {0};
    ts_ub_task_report_t ub_task_info = {0};
    struct rs_ctx_jetty_cb jetty_cb = {0};
    urma_jetty_t jetty = {0};
    struct rs_ub_dev_cb rdev_cb = {0};
    struct event_info event = {0};
    struct rs_cb rscb = {0};
    rscb.protocol = PROTOCOL_UDMA;
    int ret = 0;

    mocker_clean();
    mocker(rs_ctx_ccu_custom_channel, 10, 0);
    mocker(rs_ub_free_jetty_cb_list, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(dl_hal_esched_submit_event, 10, 0);
    mocker(rs_urma_unbind_jetty, 10, 0);
    mocker(rs_ub_ctx_drv_jetty_delete, 10, 0);
    mocker(rs_ub_ctx_free_jetty_cb, 10, 0);

    event.priv.msg_len = sizeof(struct tag_ts_hccp_msg);
    struct tag_ts_hccp_msg  *hccp_msg = (struct tag_ts_hccp_msg *)event.priv.msg;
    ccu_task_info.num = 1;
    ub_task_info.num = 1;
    RS_INIT_LIST_HEAD(&rscb.rdev_list);
    rs_list_add_tail(&rdev_cb.list, &rscb.rdev_list);
    jetty_cb.jetty = &jetty;
    jetty_cb.state = RS_JETTY_STATE_BIND;
    RS_INIT_LIST_HEAD(&rdev_cb.jetty_list);
    rs_list_add_tail(&jetty_cb.list, &rdev_cb.jetty_list);

    // ub_task not found
    hccp_msg->is_app_exit = 0;
    hccp_msg->cmd_type = 0;
    ub_task_info.array[0].jetty_id = 1;
    hccp_msg->u.ub_task_info = ub_task_info;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, 0);

    // ub_task
    hccp_msg->is_app_exit = 0;
    hccp_msg->cmd_type = 0;
    ub_task_info.array[0].jetty_id = 0;
    hccp_msg->u.ub_task_info = ub_task_info;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, 0);

    // ccu task
    hccp_msg->is_app_exit = 0;
    hccp_msg->cmd_type = 1;
    hccp_msg->u.ccu_task_info = ccu_task_info;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, 0);

    // unspport cmd
    hccp_msg->is_app_exit = 0;
    hccp_msg->cmd_type = 2;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, -EINVAL);

    // host app exit
    hccp_msg->is_app_exit = 1;
    hccp_msg->cmd_type = 1;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, 0);

    // rs_esched_handle_event ERROR
    mocker(dl_hal_esched_wait_event, 10, DRV_ERROR_INVALID_DEVICE);
    rs_esched_handle_event(&rscb);
    mocker_clean();

    mocker(dl_hal_esched_wait_event, 10, 0);
    mocker(rs_ctx_ccu_custom_channel, 10, 0);
    mocker(rs_ub_free_jetty_cb_list, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(dl_hal_esched_submit_event, 10, 0);
    mocker(rs_urma_unbind_jetty, 10, 0);
    mocker(rs_ub_ctx_drv_jetty_delete, 10, 0);
    mocker(rs_ub_ctx_free_jetty_cb, 10, 0);

    // unsupport status
    hccp_msg->is_app_exit = 2;
    hccp_msg->cmd_type = 1;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, -EINVAL);

    // ack event
    rs_esched_ack_event(&rscb, &event);

    // check len
    event.priv.msg_len = sizeof(struct tag_ts_hccp_msg) + 1;
    ret = rs_esched_process_event(&rscb, &event);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
}