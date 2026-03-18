/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu context header file
 * Create: 2025-02-18
 */

#ifndef CCU_REP_CTX_H
#define CCU_REP_CTX_H

#include <set>
#include <string>
#include <unordered_map>

#include "hcomm_primitives.h"
#include "ccu_rep_base_v1.h"
#include "ccu_rep_block_v1.h"
#include "ccu_kernel_arg.h"

#include "ccu_common.h"

namespace hcomm {
constexpr uint16_t  CCU_MAX_CHANNEL_NUM     = 16;     // 最多16条link
constexpr uint16_t  INVALID_CKE_ID          = 0xFFFF; // CKE ID非法值
constexpr uint16_t  INVALID_VALUE_CHANNELID = 0xFFFF; // channel id非法值
constexpr uint64_t       INVALID_VALUE_NOTIFYID  = 0xFFFFFFFFFFFFFFFF; // NOTIFY id非法值
constexpr int32_t INVALID_RANKID = INT32_MAX;

enum class CcuProfilinType { CCU_TASK_PROFILING, CCU_WAITCKE_PROFILING, CCU_LOOPGROUP_PROFILING, CCU_MAP_PROFILING };

struct CcuProfilingInfo {
    std::string name;          // CCU任务名或微码名
    uint8_t type;              // 枚举，0为Task粒度，1为WaitCKE，2为LoopGroup，3为channelId->RemoteRankId的映射
    uint8_t dieId;             // CCU任务执行的DieId
    uint8_t missionId;         // CCU任务执行的MissionId
    uint16_t instrId;
    uint8_t reduceOpType;      // 与HcclReduceOp类型保持一致
    uint8_t inputDataType;     // 与HcclDataType类型保持一致
    uint8_t outputDataType;    // 与HcclDataType类型保持一致
    uint64_t dataSize;         // 输入数据大小
    uint32_t ckeId;
    uint32_t mask;
    uint16_t channelId[CCU_MAX_CHANNEL_NUM];    // LoopGroup所包含的搬运指令使用的ChannelId
    uint32_t remoteRankId[CCU_MAX_CHANNEL_NUM]; // LoopGroup所包含的搬运指令的对端

    CcuProfilingInfo() : name(""), type(0), dieId(0), missionId(0), instrId(0), reduceOpType(0), inputDataType(0), outputDataType(0), dataSize(0), ckeId(0), mask(0) {
        (void)memset_s(channelId, sizeof(channelId), INVALID_VALUE_CHANNELID, sizeof(channelId));
        (void)memset_s(remoteRankId, sizeof(remoteRankId), INVALID_RANKID, sizeof(remoteRankId));
    }
};
namespace CcuRep {


struct LoopGroupProfilingInfo {
 	     std::vector<CcuProfilingInfo> ccuProfilingInfos;
 	     std::unordered_map<std::shared_ptr<CcuRep::CcuRepBase>, uint32_t> loadRep2ArgIdxMap; // loadArg rep -> argIdx
 	     std::vector<std::shared_ptr<CcuRepBase>> assignProfilingReps;  // assign rep
 	     std::vector<std::shared_ptr<CcuRepBase>> lgProfilingReps;  // loopgroup rep
 	 };

class CcuRepContext {
public:
    explicit CcuRepContext();
    virtual ~CcuRepContext();

    // 平台层内部使用
    std::shared_ptr<CcuRep::CcuRepBlock> CurrentBlock();
    void                                 SetCurrentBlock(std::shared_ptr<CcuRep::CcuRepBlock> repBlock);
    void                                 Append(std::shared_ptr<CcuRep::CcuRepBase> rep);
    const std::vector<std::shared_ptr<CcuRep::CcuRepBase>> &GetRepSequence();
    std::shared_ptr<CcuRep::CcuRepBase> GetRepByInstrId(uint16_t instrId);
    void DumpReprestation();

    void     SetDieId(uint32_t dieId);
    uint32_t GetDieId() const;
    void     SetMissionId(uint32_t missionId);
    uint32_t GetMissionId() const;
    void     SetMissionKey(uint32_t missionKey);
    uint32_t GetMissionKey() const;
    /**
        @details 获取返回的profiling信息
    */
    std::vector<CcuProfilingInfo> & GetProfilingInfo();
    /**
        @details 获取返回的loopGroup的profiling信息
    */
    LoopGroupProfilingInfo &GetLGProfilingInfo();

    /**
        @details 获取返回的waitcke的profiling信息
    */
    const std::vector<std::shared_ptr<CcuRepBase>> &GetWaiteCkeProfilingReps() const;
    /**
        @details:保存rep信息，后续有arg后配合补全profiling信息
    */
    void CollectProfilingReps(std::shared_ptr<CcuRep::CcuRepBase> rep);
    /**
        @details:添加sqe粒度的profiling信息，在ctx构造时调用
    */
    void AddSqeProfiling(const CcuCtxArg &arg);

    void AddProfiling(const std::string &name, uint32_t mask);

    void AddProfiling(const CcuTransport &transport, const std::string &name, uint32_t signalIndex, uint32_t mask);
    void AddProfiling(const CcuTransportGroup &transportGroup, const std::string &name, uint32_t signalIndex, uint32_t mask);
    void AddProfiling(const std::vector<CcuTransport*> &transports);
    void AddProfiling(const std::vector<CcuTransport *> &transports, DataType dataType, DataType outputDataType,
                      ReduceOp opType);


    // ccu profiling相关接口
    std::vector<CcuProfilingInfo> &GetProfilingInfo();
    LoopGroupProfilingInfo &GetLGProfilingInfo();
    const std::vector<std::shared_ptr<CcuRepBase>> &GetWaiteCkeProfilingReps() const;
    void CollectProfilingReps(std::shared_ptr<CcuRep::CcuRepBase> rep);

    void AddSqeProfiling(const CcuKernelArg &arg);
    void AddProfiling(const std::string &name, uint32_t mask);
    void AddProfiling(const ChannelHandle channel, const std::string &name, uint32_t signalIndex, uint32_t mask);
    // void AddProfiling(const CcuTransportGroup &transportGroup, const std::string &name, uint32_t signalIndex, uint32_t mask);
    void AddProfiling(const ChannelHandle *channels, uint32_t channelNum);
    void AddProfiling(const ChannelHandle *channels, uint32_t channelNum, HcclDataType dataType, HcclDataType outputDataType,
                    HcclReduceOp opType);
protected:
    std::set<std::string> registeredLoop;

private:
    std::shared_ptr<CcuRep::CcuRepBlock> activeBlock{nullptr};
    std::shared_ptr<CcuRep::CcuRepBlock> mainBlock{nullptr};

    uint32_t             dieId{CCU_MAX_IODIE_NUM};
    uint32_t             missionId{UINT32_MAX};
    uint32_t             missionKey{0};
    // CCU Profiling相关数据
    CcuProfilingInfo ccuProfilingInfoCache;
    std::vector<std::shared_ptr<CcuRepBase>> allLgProfilingReps;  // 当前所有的loopGroup Rep
    LoopGroupProfilingInfo lgProfilingInfo; // LoopGroup相关profiling缓存信息
    std::vector<std::shared_ptr<CcuRepBase>> waitCkeProfilingReps; // waitCKE相关REP缓存
    std::vector<CcuProfilingInfo> profilingInfo; // context全部profiling缓存信息
};

}; // namespace CcuRep
}; // namespace hcomm

#endif // _CCU_REP_CTX_H