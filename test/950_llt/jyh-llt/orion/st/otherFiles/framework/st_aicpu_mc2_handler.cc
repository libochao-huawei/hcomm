#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <thread>
#include "kfc.h"
#define private public
#define protected public
#include "internal_exception.h"
#include "aicpu_mc2_handler.h"
#include "task_exception_func.h"
#include "communicator_impl_lite_manager.h"
#include "kernel_entrance.h"
#undef private
#undef protected

using namespace Hccl;
class AicpuMc2HandlerTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        std::cout << "AicpuMc2HandlerTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AicpuMc2HandlerTest TearDown" << std::endl;
    }

protected:
    void SetUp() override {
        // 初始化代码
        kernelParam.envConfig.taskExceptionEnable = true;
        kernelParam.op.algOperator.opMode = OpMode::OPBASE;
        kernelParam.needUpdateRes = true;

        kernelParam.comm.idIndex = 0;
        kernelParam.comm.devType = DevType::DEV_TYPE_910_95;
        kernelParam.comm.myRank = 3;
        kernelParam.comm.rankSie = 10;
        kernelParam.comm.devPhyId = 0;
        kernelParam.comm.opBaseScratch.size = 3;
        kernelParam.comm.opCounterAddr = 0;

        strncpy(kernelParam.algName, "TestAlgorithm", MAX_NAME_LEN);
        strncpy(kernelParam.opTag, "TestTag", MAX_OP_TAG_LEN);

        MOCKER_CPP(&CommunicatorImplLite::UpdateCommParam).stubs().will(ignoreReturnValue());
        MOCKER_CPP(&CommunicatorImplLite::UpdateOpRes).stubs().will(ignoreReturnValue());
        MOCKER_CPP(&CommunicatorImplLite::SetDfxOpInfo).stubs().will(ignoreReturnValue());
        MOCKER_CPP(&CommunicatorImplLite::UpdateHDCommnicate).stubs().will(ignoreReturnValue());
        MOCKER_CPP(&CommunicatorImplLite::RegisterRtsqCallback).stubs().will(ignoreReturnValue());
        MOCKER_CPP(&RtsqBase::QuerySqBaseAddr).stubs().with(any()).will(returnValue(reinterpret_cast<u64>(&mockSq)));
        MOCKER_CPP(&RtsqBase::QuerySqStatusByType).stubs().with(any()).will(returnValue(0));
        MOCKER_CPP(&RtsqBase::ConfigSqStatusByType).stubs();
        std::cout << "A Test case in AicpuMc2HandlerTest SetUp" << std::endl;
    }

    void TearDown() override {
        // 清理代码
        std::cout << "A Test case in AicpuMc2HandlerTest TearDown" << std::endl;
    }

    u8 mockSq[AC_SQE_SIZE * AC_SQE_MAX_CNT]{0};
    CommunicatorImplLite communicatorImplLite{0};
    HcclKernelParamLite kernelParam;
};

TEST_F(AicpuMc2HandlerTest, test_HcclGetCommHandleByCtx_twice)
{
    void* tmpCtx = reinterpret_cast<void*>(&kernelParam);
    CommunicatorImplLite* tmpComm = reinterpret_cast<CommunicatorImplLite*>(&communicatorImplLite);
    void** comm = reinterpret_cast<void**>(&tmpComm);
    EXPECT_EQ(HCCL_SUCCESS, AicpuMc2Handler::GetInstance().HcclGetCommHandleByCtx(tmpCtx, comm));
    EXPECT_EQ(true, tmpComm->IsUsed());

    EXPECT_EQ(HCCL_E_TIMEOUT, AicpuMc2Handler::GetInstance().HcclGetCommHandleByCtx(tmpCtx, comm));
    AicpuMc2Handler::GetInstance().HcclReleaseComm(tmpComm);
    EXPECT_EQ(false, tmpComm->IsUsed());
}

TEST_F(AicpuMc2HandlerTest, test_HcclGetCommHandleByCtx_then_HcclKernelEntrance)
{
    void* tmpCtx = reinterpret_cast<void*>(&kernelParam);
    CommunicatorImplLite* tmpComm = reinterpret_cast<CommunicatorImplLite*>(&communicatorImplLite);
    void** comm = reinterpret_cast<void**>(&tmpComm);

    EXPECT_EQ(HCCL_SUCCESS, AicpuMc2Handler::GetInstance().HcclGetCommHandleByCtx(tmpCtx, comm));
    EXPECT_EQ(true, tmpComm->IsUsed());
    
    EXPECT_EQ(HCCL_E_TIMEOUT, HcclKernelEntrance(tmpCtx));
    AicpuMc2Handler::GetInstance().HcclReleaseComm(tmpComm);
}

int Mocker_GetReporterInfo_Error(TaskExceptionFunc *This, const StreamLite *curStream,std::shared_ptr<halReportRecvInfo> recvInfo) {
    recvInfo->type = static_cast<drvSqCqType_t>(DRV_LOGIC_TYPE);
    recvInfo->tsId = 0;
    recvInfo->report_cqe_num = 1;
    recvInfo->stream_id = 0;
    recvInfo->cqId = 0;
    recvInfo->timeout = 0;               // 不设置超时时间，非阻塞
    recvInfo->task_id = 0xFFFF;          // 接收所有类型
    recvInfo->cqe_num = 100;  // 单次接收的最大cqe数量
    constexpr uint32_t cqeSize = 100;
    rtLogicCqReport_t * tmpCqeAddr = reinterpret_cast<rtLogicCqReport_t *>(recvInfo->cqe_addr);
    tmpCqeAddr[0] = {1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0, 0};
    tmpCqeAddr->errorType = 1;
    return 0;
}

TEST_F(AicpuMc2HandlerTest, test_HcclGetTaskStatus_When_TaskHasException_Expect_ReturnErrorStatus)
{
    void* tmpCtx = reinterpret_cast<void*>(&kernelParam);
    CommunicatorImplLite* tmpComm = reinterpret_cast<CommunicatorImplLite*>(&communicatorImplLite);
    void** comm = reinterpret_cast<void**>(&tmpComm);
    EXPECT_EQ(HCCL_SUCCESS, AicpuMc2Handler::GetInstance().HcclGetCommHandleByCtx(tmpCtx, comm));
    MOCKER_CPP(&TaskExceptionFunc::GetReporterInfo).stubs().with(any()).will(invoke(Mocker_GetReporterInfo_Error));

    halSqCqQueryInfo queryInfo;
    queryInfo.tsId     = 0;
    queryInfo.sqId     = 0;
    queryInfo.cqId     = 0;
    queryInfo.type     = DRV_NORMAL_TYPE;
    queryInfo.prop     = DRV_SQCQ_PROP_SQ_BASE;
    queryInfo.value[0] = 0;
    queryInfo.value[1] = 0;
    MOCKER(halSqCqQuery).stubs().with(any(), outBoundP(&queryInfo, sizeof(queryInfo))).will(returnValue(0));

    vector<char> masterBuff = {
        0x00, 0x00, 0x00, 0x00,  // id
        0x00, 0x00, 0x00, 0x01,  // sqId
        0x00, 0x00, 0x00, 0x01,  // devPhyId
        0x00, 0x00, 0x00, 0x01   // cqId
    };
    u32 num = 1;
    BinaryStream binaryStream;
    binaryStream << num;
    binaryStream << masterBuff;
    std::vector<char> result;
    binaryStream.Dump(result);
    tmpComm->GetStreamLiteMgr()->ParsePackedData(result);

    HcclTaskStatus status = HcclTaskStatus::HCCL_NORMAL_STATUS;
    EXPECT_EQ(HCCL_SUCCESS, AicpuMc2Handler::GetInstance().HcclGetTaskStatus(tmpComm, &status));
    EXPECT_EQ(HcclTaskStatus::HCCL_CQE_ERROR, status);
    AicpuMc2Handler::GetInstance().HcclReleaseComm(tmpComm);
}

TEST_F(AicpuMc2HandlerTest, test_HcclOpLaunch_twice)
{
    HcclOpData data;
    data.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    data.dataType = HcclDataType::HCCL_DATA_TYPE_INT16;
    data.outputDataType = HcclDataType::HCCL_DATA_TYPE_INT16;
    data.dataDes.dataType = HcclDataType::HCCL_DATA_TYPE_INT16;
    data.dataCount = 536870912;
    data.reduceOp = HcclReduceOp::HCCL_REDUCE_MIN;
    
    data.input = 0x1000000;
    data.output = 0x2000000;
    kernelParam.op.algOperator.opType = OP_TYPE_MAP.at(HcclCMDType::HCCL_CMD_ALLREDUCE);

    void* tmpCtx = reinterpret_cast<void*>(&kernelParam);
    CommunicatorImplLite* tmpComm = reinterpret_cast<CommunicatorImplLite*>(&communicatorImplLite);
    void** comm = reinterpret_cast<void**>(&tmpComm);
    EXPECT_EQ(HCCL_SUCCESS, AicpuMc2Handler::GetInstance().HcclGetCommHandleByCtx(tmpCtx, comm));

    MOCKER_CPP(&CommunicatorImplLite::UpdateLocBuffer).stubs().with(any()).will(ignoreReturnValue());
    MOCKER_CPP(&CommunicatorImplLite::GetInsQueue).stubs().with(any()).will(returnValue(std::make_shared<InsQueue>()));
    MOCKER_CPP(&InsExecutor::ExecuteV82).stubs().will(ignoreReturnValue());
    MOCKER_CPP(&ProfilingReporterLite::ReportAllTasks).stubs().with(any()).will(ignoreReturnValue());
    EXPECT_EQ(HCCL_SUCCESS, AicpuMc2Handler::GetInstance().HcclLaunchOp(tmpComm, &data));
    EXPECT_EQ(HCCL_SUCCESS, AicpuMc2Handler::GetInstance().HcclLaunchOp(tmpComm, &data));
}

