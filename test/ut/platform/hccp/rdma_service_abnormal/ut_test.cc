/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

extern "C" {
#include "ut_dispatch.h"
#include "tc_ut_rs.h"
#include "tc_ut_rs_ping.h"
#include "user_log.h"
#include <sys/epoll.h>

extern void RsEpollEventHandleOne(struct rs_cb *rs_cb, struct epoll_event *events);
extern void RsEpollEventInHandle(struct rs_cb *rs_cb, struct epoll_event *events);

}

#include "gtest/gtest.h"
#include <stdio.h>
#include <mockcpp/mockcpp.hpp>

using namespace std;

#define RS_CHECK_POINTER_NULL_RETURN_VOID(ptr) do { \
        if ((ptr) == NULL) { \
            hccp_err("pointer is NULL!"); \
            return; \
        } \
} while (0)

void rs_epoll_event_handle_one_stub(struct rs_cb *rs_cb, struct epoll_event *events)
{
    RS_CHECK_POINTER_NULL_RETURN_VOID(events);
    RS_CHECK_POINTER_NULL_RETURN_VOID(rs_cb);
    if (events->events & EPOLLIN) {
        RsEpollEventInHandle(rs_cb, events);
    } else {
        hccp_warn("unknown event(0x%x) !", events->events);
    }

    return;
}

class RS : public testing::Test
{
protected:
   static void SetUpTestCase()
    {
        std::cout << "\033[36m--RoCE RS SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--RoCE RS TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
		MOCKER(RsEpollEventHandleOne)
		.expects(atMost(100000))
		.will(invoke(rs_epoll_event_handle_one_stub));
    }
    virtual void TearDown()
    {
	 GlobalMockObject::verify();
    }
};

TEST_M(RS, tc_rs_init2);
TEST_M(RS, tc_rs_deinit2);

TEST_M(RS, tc_rs_socket_init);
TEST_M(RS, tc_rs_socket_deinit);

TEST_M(RS, tc_rs_rdev_init);
TEST_M(RS, tc_rs_rdev_deinit);

TEST_M(RS, tc_rs_get_tsqp_depth_abnormal);
TEST_M(RS, tc_rs_set_tsqp_depth_abnormal);

TEST_M(RS, tc_rs_socket_listen_start2);
TEST_M(RS, tc_rs_qp_create2);

TEST_M(RS, tc_rs_abnormal2);

TEST_M(RS, tc_rs_mr_abnormal2);
TEST_M(RS, tc_rs_get_gid_index2);
TEST_M(RS, tc_rs_qp_connect_async2);
TEST_M(RS, tc_rs_send_wr2);

TEST_M(RS, tc_rs_socket_nodeid2vnic);
TEST_M(RS, tc_rs_server_valid_async_init);
TEST_M(RS, tc_rs_connect_handle);
TEST_M(RS, tc_rs_get_qp_context);
TEST_M(RS, tc_rs_socket_get_bind_by_chip);
TEST_M(RS, tc_rs_socket_batch_abort);
TEST_M(RS, tc_rs_tcp_recv_tag_in_handle);
TEST_M(RS, tc_rs_server_valid_async_abnormal);
TEST_M(RS, tc_rs_server_valid_async_abnormal_01);
TEST_M(RS, tc_rs_net_api_init_fail);

/* pingMesh ut cases */
TEST_M(RS, tc_rs_ping_handle_init);
TEST_M(RS, tc_rs_ping_handle_deinit);
TEST_M(RS, tc_rs_ping_init);
TEST_M(RS, tc_rs_ping_target_add);
TEST_M(RS, tc_rs_ping_task_start);
TEST_M(RS, tc_rs_ping_get_results);
TEST_M(RS, tc_rs_ping_task_stop);
TEST_M(RS, tc_rs_ping_target_del);
TEST_M(RS, tc_rs_ping_deinit);
TEST_M(RS, tc_rs_ping_urma_check_fd);
TEST_M(RS, tc_rs_ping_cb_get_ib_ctx_and_index);
