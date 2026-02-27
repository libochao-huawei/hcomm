/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * Internal implementation details for CCU.
 * DO NOT include this file directly in external code.
 */

#ifndef HCOMM_CCU_INTERNAL_H
#define HCOMM_CCU_INTERNAL_H

#include <array>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "ccu_datatype.h"
#include "ccu_resource.h"
#include "ccu_microcode_v1.h"

namespace hcomm {
namespace CcuRep {

struct TransDep {
    int32_t logicalId;
    uint16_t dieId;
    uint16_t reserveXnId;
    uint16_t reserveGsaId;
    uint16_t reserveCkeId;
    uint16_t reserveChannalId[2];
    uint64_t xnBaseAddr;
    uint64_t ccuResSpaceTokenInfo;
    uint64_t memTokenInfo;
    uint16_t commXn[3];
    uint16_t commGsa[2];
    uint16_t commSignal;
    uint16_t loadXnId;
    bool isFuncBlock;
};

class CcuRepBase {
public:
    explicit CcuRepBase();
    virtual ~CcuRepBase();
    virtual bool Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) = 0;
    virtual std::string Describe() = 0;

    CcuRepType Type() const;
    bool Translated() const;
    uint16_t StartInstrId() const;
    virtual uint16_t InstrCount();

protected:
    CcuRepType type{CcuRepType::BASE};
    bool translated{false};
    uint16_t instrId{0};
    uint16_t instrCount{0};
};

class CcuRepBlock : public CcuRepBase {
public:
    explicit CcuRepBlock(const std::string &label = "");
    bool Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;
    uint16_t InstrCount() override;
    const std::string &GetLabel() const;
    std::vector<std::shared_ptr<CcuRepBase>> &GetReps();
    void Append(std::shared_ptr<CcuRepBase> rep);
    std::shared_ptr<CcuRepBase> GetRepByInstrId(uint16_t instrId);

private:
    std::string label;
    std::vector<std::shared_ptr<CcuRepBase>> repVec;
};

class CcuRepContext {
public:
    explicit CcuRepContext();
    virtual ~CcuRepContext();

    std::shared_ptr<CcuRep::CcuRepBlock> CurrentBlock();
    void SetCurrentBlock(std::shared_ptr<CcuRep::CcuRepBlock> repBlock);
    void Append(std::shared_ptr<CcuRep::CcuRepBase> rep);
    const std::vector<std::shared_ptr<CcuRep::CcuRepBase>> &GetRepSequence();
    std::shared_ptr<CcuRep::CcuRepBase> GetRepByInstrId(uint16_t instrId);
    void DumpReprestation();

    void SetDieId(uint32_t dieId);
    uint32_t GetDieId() const;
    void SetMissionId(uint32_t missionId);
    uint32_t GetMissionId() const;
    void SetMissionKey(uint32_t missionKey);
    uint32_t GetMissionKey() const;

protected:
    std::set<std::string> registeredLoop;

private:
    std::shared_ptr<CcuRep::CcuRepBlock> activeBlock{nullptr};
    std::shared_ptr<CcuRep::CcuRepBlock> mainBlock{nullptr};
    uint32_t dieId{CCU_MAX_IODIE_NUM};
    uint32_t missionId{UINT32_MAX};
    uint32_t missionKey{0};
};

class CcuRepJumpLabel : public CcuRepBlock {
public:
    explicit CcuRepJumpLabel(const std::string &label);
    std::string Describe() override;
};

class CcuRepJumpBase : public CcuRepBase {
public:
    explicit CcuRepJumpBase(const std::string &label, const Variable &targetInstrId);
    void Reference(std::shared_ptr<CcuRepJumpLabel> refRep);

protected:
    std::string label;
    std::shared_ptr<CcuRepJumpLabel> jumpLabel{nullptr};
    Variable targetInstrId;
    CcuInstr *instr{nullptr};
};

class CcuRepJump : public CcuRepJumpBase {
public:
    explicit CcuRepJump(const std::string &label, const Variable &targetInstrId);
    bool Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;
};

class CcuRepJumpNE : public CcuRepJumpBase {
public:
    CcuRepJumpNE(const std::string &label, const Variable &targetInstrId, const Variable &condition, uint64_t expected);
    bool Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

private:
    Variable condition;
    uint64_t expected{0};
};

class CcuRepJumpEQ : public CcuRepJumpBase {
public:
    CcuRepJumpEQ(const std::string &label, const Variable &targetInstrId, const Variable &condition, uint64_t expected);
    bool Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

private:
    Variable condition;
    uint64_t expected{0};
};

class CcuRepLoopBlock : public CcuRepBlock {
public:
    explicit CcuRepLoopBlock(const std::string &label);
    std::string Describe() override;

    void DefineArg(Variable var);
    void DefineArg(LocalAddr addr);
    void DefineArg(RemoteAddr addr);
    void DefineArg(const std::vector<Variable> varList);
    void DefineArg(const std::vector<LocalAddr> addrList);
    void DefineArg(const std::vector<RemoteAddr> addrList);
    CcuRepArg &GetArg(uint16_t index);

private:
    std::vector<CcuRepArg> args;
};

class CcuRepLoopCall : public CcuRepBase {
public:
    explicit CcuRepLoopCall(const std::string &label);
    bool Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;
    uint16_t InstrCount() override;
    const std::string &GetLabel() const;

    void Reference(std::shared_ptr<CcuRepLoopBlock> refRep);

    void SetInArg(const Variable &var);
    void SetInArg(const std::vector<Variable> &varList);
    void SetInArg(const LocalAddr &addr);
    void SetInArg(const RemoteAddr &addr);
    void SetInArg(const std::vector<LocalAddr> &addrList);
    void SetInArg(const std::vector<RemoteAddr> &addrList);

private:
    std::string label;
    std::shared_ptr<CcuRepLoopBlock> loopBlock{nullptr};
    std::vector<CcuRepArg> inArgs;
    uint32_t inArgCount{0};
    uint32_t inArgInstrCount{0};
    CcuInstr *instr{nullptr};
};

class CcuRepFuncBlock : public CcuRepBlock {
public:
    explicit CcuRepFuncBlock(const std::string &label);
    bool Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;
    uint16_t InstrCount() override;

    void SetFuncManager(CcuRepReferenceManager *funcManager);
    void DefineInArg(const Variable &var);
    void DefineOutArg(const Variable &var);
    void DefineInArg(const std::vector<Variable> &varList);
    void DefineOutArg(const std::vector<Variable> &varList);
    void SetCallLayer(uint16_t callLayer);
    uint16_t GetCallLayer() const;

private:
    CcuRepReferenceManager *funcManager{nullptr};
    std::vector<CcuRepArg> inArgs;
    std::vector<CcuRepArg> outArgs;
    uint32_t inArgCount{0};
    uint32_t outArgCount{0};
    uint16_t callLayer{0};
};

class CcuRepFuncCall : public CcuRepBase {
public:
    explicit CcuRepFuncCall(const std::string &label);
    explicit CcuRepFuncCall(const Variable &funcAddrVar);
    bool Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;
    uint16_t InstrCount() override;
    const std::string &GetLabel() const;

    void Reference(std::shared_ptr<CcuRepFuncBlock> refRep);
    void SetFuncManager(CcuRepReferenceManager *funcManager);

    void SetInArg(const Variable &var);
    void SetOutArg(const Variable &var);
    void SetInArg(const std::vector<Variable> &varList);
    void SetOutArg(const std::vector<Variable> &varList);

    int32_t GetCallLayer();

private:
    CcuRepReferenceManager *funcManager{nullptr};
    std::string label;
    std::shared_ptr<CcuRepFuncBlock> funcBlock{nullptr};
    Variable funcAddrVar;
    std::vector<CcuRepArg> inArgs;
    std::vector<CcuRepArg> outArgs;
    uint32_t inArgCount{0};
    uint32_t outArgCount{0};
    CcuInstr *instr{nullptr};
};

constexpr uint16_t FUNC_ARG_MAX = 32;
constexpr uint16_t FUNC_NEST_MAX = 8;
constexpr uint16_t FUNC_CALL_LAYER_INVALID = 0xFFFF;

class CcuRepReferenceManager {
public:
    explicit CcuRepReferenceManager(uint8_t dieId);
    static CcuResReq GetResReq(uint8_t reqDieId);
    void GetRes(CcuRepResource &res);
    std::shared_ptr<CcuRepBlock> GetRefBlock(const std::string &label);
    void SetRefBlock(const std::string &label, std::shared_ptr<CcuRepBlock> refBlock);
    uint16_t GetFuncAddr(const std::string &label);
    const Variable &GetFuncCall();
    const Variable &GetFuncRet(uint16_t callLayer);
    const std::vector<Variable> &GetFuncIn();
    const std::vector<Variable> &GetFuncOut();
    void Dump() const;
    void ClearRepReference();

private:
    bool CheckValid(const std::string &label);
    bool CheckUnique(const std::string &label);

private:
    uint8_t dieId{0};
    std::unordered_map<std::string, std::shared_ptr<CcuRepBlock>> referenceMap;
    std::vector<Variable> funcCallVar;
    std::vector<Variable> funcInVar;
    std::vector<Variable> funcOutVar;
};

struct CcuInstrInfo {
    std::vector<CcuInstr> instrVec;
    uint16_t startInstrId{0};
    uint16_t instrCount{0};
    uint16_t missionStartInstrId{0};
    uint16_t missionInstrCount{0};
};

void AppendToContext(CcuRepContext *context, std::shared_ptr<CcuRep::CcuRepBase> rep);
std::shared_ptr<CcuRep::CcuRepBlock> CurrentBlock(CcuRepContext *context);
void SetCurrentBlock(CcuRepContext *context, std::shared_ptr<CcuRep::CcuRepBlock> repBlock);
Variable CreateVariable(CcuRepContext *context);

} // namespace CcuRep
} // namespace hcomm

#endif // HCOMM_CCU_INTERNAL_H
