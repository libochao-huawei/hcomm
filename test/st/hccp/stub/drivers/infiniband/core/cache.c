#include <linux/types.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/acpi.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <net/addrconf.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>
#include <rdma/ib_umem.h>

#include <rdma/ib_smi.h>
#include <rdma/ib_user_verbs.h>
#include "hns_roce_common.h"
#include "hns_roce_device.h"
#include <rdma/hns-abi.h>

#include "ut_lib.h"


union ib_gid zgid;
EXPORT_SYMBOL(zgid);

struct ib_device *tc_stub_roce_main_ib_dev = NULL;
int ib_register_device(struct ib_device *device,
		       int (*port_callback)(struct ib_device *,
				       u8, struct kobject *))
{
	tc_stub_roce_main_ib_dev = device;
	return 0;
}

void ib_unregister_device(struct ib_device *device)
{
	tc_stub_roce_main_ib_dev = NULL;
}

UT_MAP_DEFINE(ib_query_port, proc);
int ib_query_port(struct ib_device *device,
		  u8 port_num, struct ib_port_attr *port_attr)
{
	if (UT_MAP_EXIST(ib_query_port, proc, port_num)) {
		return (int)UT_MAP_FIND(ib_query_port, proc, port_num);
	}

	return -1;
}

int ib_sg_to_pages(struct ib_mr *mr, struct scatterlist *sgl, int sg_nents,
		   unsigned int *sg_offset, int (*set_page)(struct ib_mr *, u64))
{
	if (set_page) {
		return set_page(mr, 0);
	}

	return 0;
}


static struct ib_pd tc_stub_roce_main_pd;
UT_CNT_RANGE_DEFINE(ib_alloc_pd, fail);
struct ib_pd *__ib_alloc_pd(struct ib_device *device, unsigned int flags,
			    const char *caller)
{
	int ret = UT_CNT_RANGE_CHECK_AND_GET(ib_alloc_pd, fail, 1, NULL);

	if (ret) {
		return NULL;
	}

	tc_stub_roce_main_pd.device = device;
	return &tc_stub_roce_main_pd;
}

UT_CNT_RANGE_DEFINE(ib_modify_qp_is_ok, fail);
bool ib_modify_qp_is_ok(enum ib_qp_state cur_state, enum ib_qp_state next_state,
			enum ib_qp_type type, enum ib_qp_attr_mask mask,
			enum rdma_link_layer ll)
{
	int ret = UT_CNT_RANGE_CHECK_AND_GET(ib_modify_qp_is_ok, fail, 1, NULL);

	if (ret) {
		return false;
	}

	return true;
}

UT_MAP_DEFINE(ib_umem_page_count, proc);
int ib_umem_page_count(struct ib_umem *umem)
{
	return (int)UT_MAP_FIND(ib_umem_page_count, proc, umem);
}

UT_MAP_DEFINE(ib_get_cached_gid, proc);
UT_MAP_DEFINE(ib_get_cached_gid, fail);
int ib_get_cached_gid(struct ib_device    *device,
		      u8                   port_num,
		      int                  index,
		      union ib_gid        *gid,
		      struct ib_gid_attr  *attr)
{
	if (UT_MAP_EXIST(ib_get_cached_gid, fail, port_num)) {
		return -1;
	}

	if (UT_MAP_EXIST(ib_get_cached_gid, proc, port_num)) {
		struct ib_gid_attr *attr_srq = (struct ib_gid_attr *)UT_MAP_FIND(ib_get_cached_gid, proc, port_num);

		if (attr && attr_srq) {
			*attr = *attr_srq;
		}
	}

	if (index == 0) {
		int j;
		for (j = 0; j < 16; j++)
			gid->raw[j] = 0x00;
	}

	if (index == 1) {
		struct net_device ndev;
		int i;

		attr->ndev = &ndev;
		ndev.priv_flags = IFF_802_1Q_VLAN;
		attr->gid_type = IB_GID_TYPE_ROCE_UDP_ENCAP;

		gid->raw[0] = 0x12;
		for (i = 1; i < 16; i++)
			gid->raw[i] = 0x00;
	}

	return 0;
}

UT_CNT_RANGE_DEFINE(ib_copy_to_udata, fail);
int ib_copy_to_udata(struct ib_udata *udata, void *src, size_t len)
{
	if (UT_CNT_RANGE_CHECK_AND_GET(ib_copy_to_udata, fail, 1, NULL)) {
		return -1;
	}

	if (udata && udata->outbuf && src && len) {
		memcpy(udata->outbuf, src, len);
	}

	return 0;
}

int ib_copy_from_udata(void *dest, struct ib_udata *udata, size_t len)
{
	if (udata && udata->outbuf && dest && len) {
		memcpy(dest, udata->inbuf, len);
	}

	return 0;
}

UT_CNT_RANGE_DEFINE(ib_umem_get, fail);
UT_MAP_DEFINE(ib_umem_get, proc);
struct ib_umem *ib_umem_get(struct ib_ucontext *context,
			    unsigned long addr, size_t size,
			    int access, int dmasync)
{
	if (UT_CNT_RANGE_CHECK_AND_GET(ib_umem_get, fail, 1, NULL)) {
		return ERR_PTR(-EINVAL);
	}

	return (struct ib_umem *)UT_MAP_FIND(ib_umem_get, proc, addr);
}

static struct ib_cq tc_stub_roce_main_cq;
UT_CNT_RANGE_DEFINE(ib_create_cq, fail);
struct ib_cq *ib_create_cq(struct ib_device *device,
			   ib_comp_handler comp_handler,
			   void (*event_handler)(struct ib_event *, void *),
			   void *cq_context,
			   const struct ib_cq_init_attr *cq_attr)
{
	int ret = UT_CNT_RANGE_CHECK_AND_GET(ib_create_cq, fail, 1, NULL);

	if (ret) {
		return NULL;
	}

	return &tc_stub_roce_main_cq;
}


