/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * CCU Control Flow Interfaces
 */

#ifndef HCOMM_CCU_CONTROL_FLOW_H
#define HCOMM_CCU_CONTROL_FLOW_H

#include <string>
#include <vector>
#include "ccu_datatype.h"

namespace hcomm {
namespace CcuRep {

class FuncCall {
public:
    FuncCall(CcuRepContext *context, std::string label);
    FuncCall(CcuRepContext *context, const Variable &funcAddr);

    template <typename... Arguments>
    FuncCall &operator()(const Arguments &...args) {
        SetArgHelper(args...);
        AppendToContext();
        return *this;
    }

    FuncCall &operator()() {
        AppendToContext();
        return *this;
    }

    void AppendToContext();

    template <typename T>
    void SetInArg(T &&arg) {
        repFuncCall->SetInArg(std::forward<T>(arg));
    }

    template <typename T>
    void SetOutArg(T &&arg) {
        repFuncCall->SetOutArg(std::forward<T>(arg));
    }

private:
    template <typename First>
    void SetArgHelper(const First &first) {
        repFuncCall->SetInArg(first);
    }

    template <typename First, typename... Rest>
    void SetArgHelper(const First &first, const Rest &...rest) {
        repFuncCall->SetInArg(first);
        SetArgHelper(rest...);
    }

    CcuRepContext *context{nullptr};
    std::string label;
    std::shared_ptr<CcuRepFuncCall> repFuncCall{nullptr};
};

class LoopCall {
public:
    LoopCall(CcuRepContext *context, const std::string &label);
    const std::string &GetLabel() const { return label; }

    template <typename... Arguments>
    LoopCall &operator()(const Arguments &...args) {
        SetArgHelper(args...);
        AppendToContext();
        return *this;
    }

    LoopCall &operator()() {
        AppendToContext();
        return *this;
    }

    void AppendToContext();

private:
    template <typename First>
    void SetArgHelper(const First &first) {
        repLoopCall->SetInArg(first);
    }

    template <typename First, typename... Rest>
    void SetArgHelper(const First &first, const Rest &...rest) {
        repLoopCall->SetInArg(first);
        SetArgHelper(rest...);
    }

    CcuRepContext *context{nullptr};
    std::string label;
    std::shared_ptr<CcuRepLoopCall> repLoopCall{nullptr};
};

class LoopBlock {
public:
    LoopBlock(CcuRepContext *context, std::string label);
    ~LoopBlock();

    template <typename... Arguments>
    LoopBlock &operator()(const Arguments &...args) {
        DefineInArgHelper(args...);
        return *this;
    }

private:
    template <typename First>
    void DefineInArgHelper(const First &first) {
        repLoopBlock->DefineArg(first);
    }

    template <typename First, typename... Rest>
    void DefineInArgHelper(const First &first, const Rest &...rest) {
        repLoopBlock->DefineArg(first);
        DefineInArgHelper(rest...);
    }

    CcuRepContext *context{nullptr};
    std::string label;
    std::shared_ptr<CcuRepLoopBlock> repLoopBlock{nullptr};
    std::shared_ptr<CcuRepBlock> curActiveBlock{nullptr};
};

class LoopGroupCall {
public:
    explicit LoopGroupCall(CcuRepContext *context, std::string label = "") : context(context), label(label) {}
    void Run(const std::vector<LoopCall> &loopVec, const std::vector<Variable> &loopCfg,
             const std::vector<Executor> &executors, Variable paraCfgIn, Variable offsetCfgIn) const;

private:
    CcuRepContext *context;
    std::string label;
    uint64_t paraCfg{0};
    uint64_t offsetCfg{0};
};

class Repeat {
public:
    Repeat(CcuRepContext *context, CcuRelationalOperator<Variable, uint64_t> rel);
    ~Repeat();
    void Break();
    bool Check() const;
    void Run();

private:
    CcuRepContext *context{nullptr};
    bool isExecuted{false};
    std::shared_ptr<CcuRepJumpBase> jump{nullptr};
    std::shared_ptr<CcuRepJumpLabel> beginLabel{nullptr};
    std::shared_ptr<CcuRepJumpLabel> endLabel{nullptr};
};

class Condition {
public:
    Condition(CcuRepContext *context, CcuRelationalOperator<Variable, uint64_t> rel);
    ~Condition();
    bool Check() const;
    void Run();

private:
    CcuRepContext *context{nullptr};
    bool isExecuted{false};
    std::shared_ptr<CcuRepJumpBase> jump{nullptr};
    std::shared_ptr<CcuRepJumpLabel> endLabel{nullptr};
};

#define CCU_WHILE(x) \
    for (auto __ccuRepeatHidden = CcuRep::Repeat(this, x); __ccuRepeatHidden.Check(); __ccuRepeatHidden.Run())
#define CCU_BREAK __ccuRepeatHidden.Break()

#define CCU_IF(x) CCU_IF_HELPER1(__COUNTER__, x)
#define CCU_IF_HELPER1(ctr, x) CCU_IF_HELPER2(ctr, x)
#define CCU_IF_HELPER2(ctr, x) \
    for (auto __ccuConditionHidden##ctr = CcuRep::Condition(this, x); __ccuConditionHidden##ctr.Check(); __ccuConditionHidden##ctr.Run())

} // namespace CcuRep
} // namespace hcomm

#endif // HCOMM_CCU_CONTROL_FLOW_H
