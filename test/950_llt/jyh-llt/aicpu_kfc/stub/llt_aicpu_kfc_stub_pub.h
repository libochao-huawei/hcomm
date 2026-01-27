/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : llt_aicpu_kfc_stub_pub.h
  版 本 号   : 初稿
  作    者   : l
  生成日期   : 2018年03月08日
  最近修改   :
  功能描述   : st_hccl_stub.cc 的公共头文件.
  函数列表   :
  修改历史   :
  1.日    期   : 2018年03月08日
    作    者   : l
    修改内容   : 创建文件

******************************************************************************/

/*----------------------------------------------*
 * 外部变量说明                                 *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 外部函数原型说明                             *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 内部函数原型说明                             *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 全局变量                                     *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 模块级变量                                   *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 常量定义                                     *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 宏定义                                       *
 *----------------------------------------------*/

#ifndef __LLT_AICPU_KFC_STUB_PUB_H__
#define __LLT_AICPU_KFC_STUB_PUB_H__

#include "../../../hccl/whole/hccl/platform/common/sal.h" //include private .h files
#include "topoinfo_struct.h"
#include "llt_aicpu_kfc_stub_sal_pub.h"
#include "llt_aicpu_kfc_stub_fp16.h"
#define private public
#define protected public
#include "transport_heterog_pub.h" 
#include "hccl_comm_pub.h"
#include "transport_base_pub.h"
#include "network_manager_pub.h"
#include "hcom.h"
#include "hccl_ip_address.h"
#include "hccl_socket.h"
#include "hccl_socket_manager.h"
#include "adapter_hccp_common.h"
#include <runtime/rt.h>
#include <acl/acl_rt.h>

s32 log_level_get_stub();
void log_level_set_stub(s32 log_level);
void set_chip_type_stub(s32 devId, s32 chipType);
void set_chip_peer_stub(s32 devId1, s32 devId2, bool can_peer);
s32 get_data_size(const HcclDataType dataType);
void set_devphyid_to_sdId_stub(std::vector<hccl::RankInfo_t> &rankList);
HcclResult stub_hrtRaGetSingleSocketVnicIpInfo(u32 phy_id, DeviceIdType deviceIdType, u32 deviceId,  hccl::HcclIpAddress &vnicIP);
#ifdef __cplusplus
extern "C" {
#endif
// stream task类型
typedef enum task_type
{
    TASK_TYPE_MEMCPY  = 0,
    TASK_TYPE_RECORD,
    TASK_TYPE_MULTIDEV_RECORD,
    TASK_TYPE_WAIT,
    TASK_TYPE_REDUCE,
    TASK_TYPE_USLEEP,
    TASK_TYPE_NOTIFY_RECORD,
    TASK_TYPE_NOTIFY_WAIT,
    TASK_TYPE_RDMA_SEND,
    TASK_TYPE_CALLBACK_FUNC,
    TASK_TYPE_RESERVED
} tasktype_e;
extern void setTargetPort(u16 port_nodes_number, u16 port_number);
extern void ra_set_shm_name(const char* name);
extern int ra_set_work_mode(int mode);
extern void ra_set_test_type(u32 type, const char* name);
extern int setDevPhyId(uint32_t devIndex);
extern int set_board_id(unsigned int board_id);
extern int dsmi_get_board_id(int device_id, unsigned int *board_id);
extern int force_set_link_check_result(u32 device_id, s32 role, s32 index, s32 result, s32 haveGet);
extern int RaCqCreate(void *rdev_handle, struct CqAttr *attr);
extern int RaCqDestroy(void *rdev_handle, struct CqAttr *attr);
extern int RaIsFirstUsed(int ins_id);
extern int RaIsLastUsed(int ins_id);
extern int RaNormalQpCreate(void *rdev_handle, struct ibv_qp_init_attr *qp_init_attr, void **qp_handle, void** qp);
extern int RaNormalQpDestroy(void *qp_handle);
extern int RaSetQpAttrQos(void *qpHandle, struct QosAttr *attr);
extern int RaSetQpAttrTimeout(void *qpHandle, u32 *timeout);
extern int RaSetQpAttrRetryCnt(void *qpHandle, u32 *retry_cnt);
extern int RaCreateCompChannel(const void *rdma_handle, void **comp_channel);
extern int RaDestroyCompChannel(const void *rdma_handle, void *comp_channel);
extern int RaGetCqeErrInfo(unsigned int phy_id, struct CqeErrInfo *info);
extern int RaGetQpAttr(void *qp_handle, struct QpAttr *attr);
extern int RaCreateSrq(const void*, struct SrqAttr *);
extern int RaDestroySrq(const void*, struct SrqAttr *);
extern int RaQpCreateWithAttrs(void *rdev_handle, struct QpExtAttrs *ext_attrs, void **qp_handle);
extern int RaAiQpCreate(void *rdma_handle, struct QpExtAttrs *qp_attrs, struct AiQpInfo *info, void **qpHandle);
extern int RaRdevGetSupportLite(void *rdma_handle, int *support_lite);
extern int RaTypicalQpCreate(void *rdev_handle, int flag, int qp_mode, struct TypicalQp *qp_info, void **qp_handle);
extern int RaTypicalQpModify(void *rdev_handle, struct TypicalQp *local_qp_info, struct TypicalQp *remote_qp_info);
extern int RaTypicalSendWr(void *qp_handle, struct SendWr *wr, struct SendWrRsp *op_rsp);
extern int RaSocketAcceptCreditAdd(struct SocketListenInfoT conn[], unsigned int num, unsigned int creditLimit);
extern int stub_ibv_ext_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr,
	struct ibv_send_wr **bad_wr, struct ibv_post_send_ext_attr *ext_attr,
	struct ibv_post_send_ext_resp *ext_resp);
extern int stub_ibv_exp_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
    struct wr_exp_rsp *exp_rsp);
extern int RaRemapMr(const void *rdmaHandle, struct MemRemapInfo info[], unsigned int num);
extern int RaGetTlsEnable(struct RaInfo *info, bool *tls_enable);
// extern int ra_get_device_capability(const void*, struct device_cap_info *);
int GetInfoFromHandle(void *handle, u32 &deviceId, u32 &localIp, u32& idx);
HcclResult hrtGetDeviceTypeStub(DevType &devType);
#ifdef __cplusplus
}
#endif
// 用于设置本进程内device所在的网络平面，若不设置，默认按照deviceId进行
HcclResult SetDevicePlaneId(u32 devicePhyId, u32 planeId);
HcclResult GetDevicePlaneId(u32 devicePhyId, u32 &planeId);
void ClearDevicePlaneId();
int set_VM(unsigned int VMModel);
void FailureInjectStub(u32 deviceId, tasktype_e taskType);
void SetRtCallbackModleStub(u32 model);
void UseRealPortAndName(bool isUse);
bool IsUseRealPortAndName();
__attribute__((constructor)) void CallBackInitRts();
namespace hccl {
class TransportHeterogStub : public TransportHeterog {
public:
// TransportHeterogStub() : TransportHeterog("12315", invalidIp, invalidIp, 0, 0, TransportResourceInfo())
// {
// }
explicit TransportHeterogStub();
~TransportHeterogStub() = default;
virtual HcclResult Init() override
{
    return HCCL_SUCCESS;
}
virtual HcclResult Deinit() override
{
    return HCCL_SUCCESS;
}
virtual HcclResult Isend(const TransData &sendData, const TransportEndPointParam &epParam,
    HcclRequestInfo *&request) override
{
    HCCL_ERROR("11111111111111 %p", this);
    request = &request_;
    request_.transportHandle = this;
    return HCCL_SUCCESS;
}
virtual HcclResult Send(const TransData &sendData, const TransportEndPointParam &epParam) override
{
    return HCCL_SUCCESS;
}
virtual HcclResult Improbe(const TransportEndPointParam &epParam, s32 &matched, HcclMessageInfo *&msg,
    HcclStatus &status) override
{
    HCCL_ERROR("11111111111111");
    matched = 1;
    msg = &msg_;
    return HCCL_SUCCESS;
}
virtual HcclResult Improbe(const TransportEndPointParam &epParam, s32 &matched, HcclMessageInfo *&msg,
    HcclStatus &status, bool &flag) override
{
    HCCL_ERROR("11111111111111");
    matched = 1;
    msg = &msg_;
    return HCCL_SUCCESS;
}
virtual HcclResult Imrecv(const TransData &recvData, HcclMessageInfo &msg, HcclRequestInfo *&request) override
{
    HCCL_ERROR("11111111111111 %p", this);
    request = &request_;
    request_.transportHandle = this;
    return HCCL_SUCCESS;
}
virtual HcclResult Imrecv(const TransData &recvData, HcclMessageInfo &msg, HcclRequestInfo *&request,
    bool flag, bool needRecordFlag) override
{
    HCCL_ERROR("11111111111111 %p", this);
    request = &request_;
    request_.transportHandle = this;
    return HCCL_SUCCESS;
}
virtual HcclResult Test(HcclRequestInfo &request, s32 &flag, HcclStatus &compState) override
{
    HCCL_ERROR("Test11111111111111");
    flag = 1;
    return HCCL_SUCCESS;
}

private:
virtual HcclResult EnterStateProcess(ConnState nextState)
{
    return HCCL_SUCCESS;
}
virtual HcclResult LoopStateProcess()
{
    return HCCL_SUCCESS;
}

public:
HcclMessageInfo msg_;
HcclRequestInfo request_;
// HcclIpAddress invalidIp;
};


class TransportShmEventStub : public TransportHeterog {
public:
// TransportShmEventStub() : TransportHeterog("12315", invalidIp, invalidIp, 0, 0, TransportResourceInfo())
// {
// }
explicit TransportShmEventStub();
~TransportShmEventStub()
{

}
HcclResult Init() override
{
    return HCCL_SUCCESS;
}
HcclResult Deinit() override
{
    return HCCL_SUCCESS;
}
HcclResult Isend(const TransData &sendData, const TransportEndPointParam &epParam,
    HcclRequestInfo *&request) override
{
    HCCL_ERROR("11111111111111 %p", this);
    request = &request_;
    request_.transportHandle = this;
    return HCCL_SUCCESS;
}
HcclResult Send(const TransData &sendData, const TransportEndPointParam &epParam) override
{
    return HCCL_SUCCESS;
}
HcclResult Improbe(const TransportEndPointParam &epParam, s32 &matched, HcclMessageInfo *&msg,
    HcclStatus &status) override
{
    HCCL_ERROR("11111111111111");
    matched = 1;
    msg = &msg_;
    return HCCL_SUCCESS;
}
HcclResult Improbe(const TransportEndPointParam &epParam, s32 &matched, HcclMessageInfo *&msg,
    HcclStatus &status, bool &flag) override
{
    HCCL_ERROR("11111111111111");
    matched = 1;
    msg = &msg_;
    return HCCL_SUCCESS;
}
HcclResult Imrecv(const TransData &recvData, HcclMessageInfo &msg, HcclRequestInfo *&request) override
{
    HCCL_ERROR("11111111111111 %p", this);
    request = &request_;
    request_.transportHandle = this;
    return HCCL_SUCCESS;
}
HcclResult Imrecv(const TransData &recvData, HcclMessageInfo &msg, HcclRequestInfo *&request,
    bool flag, bool needRecordFlag) override
{
    HCCL_ERROR("11111111111111 %p", this);
    request = &request_;
    request_.transportHandle = this;
    return HCCL_SUCCESS;
}
HcclResult Test(HcclRequestInfo &request, s32 &flag, HcclStatus &compState) override
{
    HCCL_ERROR("Test11111111111111");
    flag = 1;
    return HCCL_SUCCESS;
}

private:
HcclResult EnterStateProcess(ConnState nextState)
{
    return HCCL_SUCCESS;
}
HcclResult LoopStateProcess()
{
    return HCCL_SUCCESS;
}

public:
HcclMessageInfo msg_;
HcclRequestInfo request_;
// HcclIpAddress invalidIp;
};
}
#endif /* __LLT_AICPU_KFC_STUB_PUB_H__ */

