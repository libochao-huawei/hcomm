#ifndef _STUB_IB_H_
#define _STUB_IB_H_

extern UT_MAP_DECLARE(ib_get_cached_gid, fail);
extern UT_MAP_DECLARE(ib_get_cached_gid, proc);

extern UT_CNT_RANGE_DECLARE(ib_modify_qp_is_ok, fail);
extern UT_MAP_DECLARE(ib_query_port, proc);
extern UT_MAP_DECLARE(ib_umem_get, proc);
extern UT_MAP_DECLARE(ib_umem_page_count, proc);
extern UT_CNT_RANGE_DECLARE(ib_umem_get, fail);
extern UT_CNT_RANGE_DECLARE(ib_copy_to_udata, fail);

extern UT_CNT_RANGE_DECLARE(ib_alloc_pd, fail);
extern UT_CNT_RANGE_DECLARE(ib_create_cq, fail);


extern struct ib_device* tc_stub_roce_main_ib_dev;

#endif
