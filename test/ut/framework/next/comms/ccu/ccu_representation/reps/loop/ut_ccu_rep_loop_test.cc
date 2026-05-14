/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <climits>

#include "ccu_rep_base_v1.h"
#include "ccu_rep_block_v1.h"
#include "ccu_rep_arg_v1.h"
#include "ccu_rep_loopblock_v1.h"
#include "ccu_rep_loopcall_v1.h"
#include "ccu_rep_loop_v1.h"
#include "ccu_rep_loopgroup_v1.h"
#include "ccu_rep_setloop_v1.h"
#include "ccu_datatype_v1.h"
#include "ccu_microcode_v1.h"
#include "ccu_assist_v1.h"
#include "ccu_api_exception.h"

namespace hcomm {
namespace CcuRep {
namespace {

class CcuRepLoopTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(CcuRepLoopTest, Constructor)
{
    Variable loopParam(nullptr);
    CcuRepLoop loop("test_loop", loopParam);

    EXPECT_EQ(loop.GetLabel(), "test_loop");
    EXPECT_EQ(loop.Type(), CcuRepType::LOOP);
}

TEST_F(CcuRepLoopTest, GetLoopParam)
{
    Variable loopParam(nullptr);
    CcuRepLoop loop("test_loop", loopParam);

    Variable* param = loop.GetLoopParam();
    EXPECT_NE(param, nullptr);
}

TEST_F(CcuRepLoopTest, Reference)
{
    Variable loopParam(nullptr);
    CcuRepLoop loop("test_loop", loopParam);
    auto loopBlock = std::make_shared<CcuRepLoopBlock>("loop_block");

    loop.Reference(loopBlock);

    EXPECT_EQ(loop.GetLoopBlock(), loopBlock.get());
}

TEST_F(CcuRepLoopTest, SetLoopParam)
{
    Variable loopParam(nullptr);
    CcuRepLoop loop("test_loop", loopParam);

    Executor executor(nullptr);
    Variable var(nullptr);
    auto setLoop = loop.SetLoopParam(executor, var);

    EXPECT_NE(setLoop, nullptr);
    EXPECT_EQ(setLoop->Type(), CcuRepType::SET_LOOP);
}

TEST_F(CcuRepLoopTest, Describe)
{
    Variable loopParam(nullptr);
    CcuRepLoop loop("test_loop", loopParam);

    std::string desc = loop.Describe();
    EXPECT_NE(desc.find("test_loop"), std::string::npos);
}

TEST_F(CcuRepLoopTest, Translate)
{
    Variable loopParam(nullptr);
    CcuRepLoop loop("test_loop", loopParam);

    auto loopBlock = std::make_shared<CcuRepLoopBlock>("loop_block");
    loop.Reference(loopBlock);

    CcuInstr instr {};
    CcuInstr* instrPtr = &instr;
    uint16_t instrId = 0;
    TransDep dep {};

    loopBlock->Translate(instrPtr, instrId, dep);
    instrPtr = &instr;
    instrId = 0;

    bool result = loop.Translate(instrPtr, instrId, dep);

    EXPECT_TRUE(result);
    EXPECT_EQ(instrId, 1U);
}

class CcuRepLoopBlockTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(CcuRepLoopBlockTest, Constructor)
{
    CcuRepLoopBlock loopBlock("test_block");

    EXPECT_EQ(loopBlock.GetLabel(), "test_block");
    EXPECT_EQ(loopBlock.Type(), CcuRepType::LOOP_BLOCK);
}

TEST_F(CcuRepLoopBlockTest, Describe)
{
    CcuRepLoopBlock loopBlock("test_block");

    std::string desc = loopBlock.Describe();
    EXPECT_NE(desc.find("test_block"), std::string::npos);
}

TEST_F(CcuRepLoopBlockTest, DefineArg_Variable)
{
    CcuRepLoopBlock loopBlock("test_block");
    Variable var(nullptr);

    loopBlock.DefineArg(var);

    CcuRepArg& arg = loopBlock.GetArg(0);
    EXPECT_EQ(arg.type, CcuArgType::VARIABLE);
}

TEST_F(CcuRepLoopBlockTest, DefineArg_Memory)
{
    CcuRepLoopBlock loopBlock("test_block");
    Address addr(nullptr);
    Variable token(nullptr);
    Memory mem(addr, token);

    loopBlock.DefineArg(mem);

    CcuRepArg& arg = loopBlock.GetArg(0);
    EXPECT_EQ(arg.type, CcuArgType::MEMORY);
}

TEST_F(CcuRepLoopBlockTest, DefineArg_LocalAddr)
{
    CcuRepLoopBlock loopBlock("test_block");
    Address addr(nullptr);
    Variable token(nullptr);
    LocalAddr localAddr(addr, token);

    loopBlock.DefineArg(localAddr);

    CcuRepArg& arg = loopBlock.GetArg(0);
    EXPECT_EQ(arg.type, CcuArgType::LOCAL_ADDR);
}

TEST_F(CcuRepLoopBlockTest, DefineArg_RemoteAddr)
{
    CcuRepLoopBlock loopBlock("test_block");
    Address addr(nullptr);
    Variable token(nullptr);
    RemoteAddr remoteAddr(addr, token);

    loopBlock.DefineArg(remoteAddr);

    CcuRepArg& arg = loopBlock.GetArg(0);
    EXPECT_EQ(arg.type, CcuArgType::REMOTE_ADDR);
}

TEST_F(CcuRepLoopBlockTest, DefineArg_VariableList)
{
    CcuRepLoopBlock loopBlock("test_block");
    std::vector<Variable> varList = {Variable(nullptr), Variable(nullptr)};

    loopBlock.DefineArg(varList);

    CcuRepArg& arg = loopBlock.GetArg(0);
    EXPECT_EQ(arg.type, CcuArgType::VARIABLE_LIST);
    EXPECT_EQ(arg.varList.size(), 2U);
}

TEST_F(CcuRepLoopBlockTest, DefineArg_MemoryList)
{
    CcuRepLoopBlock loopBlock("test_block");
    Address addr(nullptr);
    Variable token(nullptr);
    std::vector<Memory> memList = {Memory(addr, token), Memory(addr, token)};

    loopBlock.DefineArg(memList);

    CcuRepArg& arg = loopBlock.GetArg(0);
    EXPECT_EQ(arg.type, CcuArgType::MEMORY_LIST);
    EXPECT_EQ(arg.memList.size(), 2U);
}

TEST_F(CcuRepLoopBlockTest, DefineArg_LocalAddrList)
{
    CcuRepLoopBlock loopBlock("test_block");
    Address addr(nullptr);
    Variable token(nullptr);
    std::vector<LocalAddr> addrList = {LocalAddr(addr, token), LocalAddr(addr, token)};

    loopBlock.DefineArg(addrList);

    CcuRepArg& arg = loopBlock.GetArg(0);
    EXPECT_EQ(arg.type, CcuArgType::LOCAL_ADDR_LIST);
    EXPECT_EQ(arg.localAddrList.size(), 2U);
}

TEST_F(CcuRepLoopBlockTest, DefineArg_RemoteAddrList)
{
    CcuRepLoopBlock loopBlock("test_block");
    Address addr(nullptr);
    Variable token(nullptr);
    std::vector<RemoteAddr> addrList = {RemoteAddr(addr, token), RemoteAddr(addr, token)};

    loopBlock.DefineArg(addrList);

    CcuRepArg& arg = loopBlock.GetArg(0);
    EXPECT_EQ(arg.type, CcuArgType::REMOTE_ADDR_LIST);
    EXPECT_EQ(arg.remoteAddrList.size(), 2U);
}

TEST_F(CcuRepLoopBlockTest, GetArg_OutOfRange)
{
    CcuRepLoopBlock loopBlock("test_block");

    EXPECT_THROW(loopBlock.GetArg(0), Hccl::CcuApiException);
}

class CcuRepLoopCallTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(CcuRepLoopCallTest, Constructor)
{
    CcuRepLoopCall loopCall("test_call");

    EXPECT_EQ(loopCall.GetLabel(), "test_call");
    EXPECT_EQ(loopCall.Type(), CcuRepType::LOOP_CALL);
}

TEST_F(CcuRepLoopCallTest, Reference)
{
    CcuRepLoopCall loopCall("test_call");
    auto loopBlock = std::make_shared<CcuRepLoopBlock>("loop_block");

    loopCall.Reference(loopBlock);

    EXPECT_EQ(loopCall.GetLabel(), "test_call");
}

TEST_F(CcuRepLoopCallTest, SetInArg_Variable)
{
    CcuRepLoopCall loopCall("test_call");
    Variable var(nullptr);

    loopCall.SetInArg(var);

    EXPECT_EQ(loopCall.InstrCount(), 1U);
}

TEST_F(CcuRepLoopCallTest, SetInArg_VariableList)
{
    CcuRepLoopCall loopCall("test_call");
    std::vector<Variable> varList = {Variable(nullptr), Variable(nullptr)};

    loopCall.SetInArg(varList);

    EXPECT_EQ(loopCall.InstrCount(), 2U);
}

TEST_F(CcuRepLoopCallTest, SetInArg_Memory)
{
    CcuRepLoopCall loopCall("test_call");
    Address addr(nullptr);
    Variable token(nullptr);
    Memory mem(addr, token);

    loopCall.SetInArg(mem);

    EXPECT_EQ(loopCall.InstrCount(), 2U);
}

TEST_F(CcuRepLoopCallTest, SetInArg_MemoryList)
{
    CcuRepLoopCall loopCall("test_call");
    Address addr(nullptr);
    Variable token(nullptr);
    std::vector<Memory> memList = {Memory(addr, token), Memory(addr, token)};

    loopCall.SetInArg(memList);

    EXPECT_EQ(loopCall.InstrCount(), 4U);
}

TEST_F(CcuRepLoopCallTest, SetInArg_LocalAddr)
{
    CcuRepLoopCall loopCall("test_call");
    Address addr(nullptr);
    Variable token(nullptr);
    LocalAddr localAddr(addr, token);

    loopCall.SetInArg(localAddr);

    EXPECT_EQ(loopCall.InstrCount(), 2U);
}

TEST_F(CcuRepLoopCallTest, SetInArg_RemoteAddr)
{
    CcuRepLoopCall loopCall("test_call");
    Address addr(nullptr);
    Variable token(nullptr);
    RemoteAddr remoteAddr(addr, token);

    loopCall.SetInArg(remoteAddr);

    EXPECT_EQ(loopCall.InstrCount(), 2U);
}

TEST_F(CcuRepLoopCallTest, SetInArg_LocalAddrList)
{
    CcuRepLoopCall loopCall("test_call");
    Address addr(nullptr);
    Variable token(nullptr);
    std::vector<LocalAddr> addrList = {LocalAddr(addr, token), LocalAddr(addr, token)};

    loopCall.SetInArg(addrList);

    EXPECT_EQ(loopCall.InstrCount(), 4U);
}

TEST_F(CcuRepLoopCallTest, SetInArg_RemoteAddrList)
{
    CcuRepLoopCall loopCall("test_call");
    Address addr(nullptr);
    Variable token(nullptr);
    std::vector<RemoteAddr> addrList = {RemoteAddr(addr, token), RemoteAddr(addr, token)};

    loopCall.SetInArg(addrList);

    EXPECT_EQ(loopCall.InstrCount(), 4U);
}

TEST_F(CcuRepLoopCallTest, Describe)
{
    CcuRepLoopCall loopCall("test_call");

    std::string desc = loopCall.Describe();
    EXPECT_NE(desc.find("test_call"), std::string::npos);
}

TEST_F(CcuRepLoopCallTest, Translate)
{
    auto loopBlock = std::make_shared<CcuRepLoopBlock>("loop_block");
    Address addr(nullptr);
    Variable token(nullptr);
    loopBlock->DefineArg(Variable(nullptr));
    loopBlock->DefineArg(LocalAddr(addr, token));

    CcuRepLoopCall loopCall("test_call");
    loopCall.Reference(loopBlock);
    Variable var(nullptr);
    loopCall.SetInArg(var);
    loopCall.SetInArg(LocalAddr(addr, token));

    CcuInstr instr[10] = {};
    CcuInstr* instrPtr = instr;
    uint16_t instrId = 0;
    TransDep dep {0, 0, 1, 1, 0, {0, 0}, 0, 0, 0, {0, 0, 0}, {0, 0}, 0, false};

    loopBlock->Translate(instrPtr, instrId, dep);
    instrPtr = instr;
    instrId = 0;

    bool result = loopCall.Translate(instrPtr, instrId, dep);

    EXPECT_TRUE(result);
}

class CcuRepLoopGroupTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(CcuRepLoopGroupTest, Constructor)
{
    Variable parallelParam(nullptr);
    Variable offsetParam(nullptr);
    CcuRepLoopGroup loopGroup(parallelParam, offsetParam);

    EXPECT_EQ(loopGroup.Type(), CcuRepType::LOOPGROUP);
}

TEST_F(CcuRepLoopGroupTest, GetOffestParam)
{
    Variable parallelParam(nullptr);
    Variable offsetParam(nullptr);
    CcuRepLoopGroup loopGroup(parallelParam, offsetParam);

    Variable offset = loopGroup.GetOffestParam();
    EXPECT_EQ(offset.Id(), offsetParam.Id());
}

TEST_F(CcuRepLoopGroupTest, SetParallelParam)
{
    Variable parallelParam(nullptr);
    Variable offsetParam(nullptr);
    CcuRepLoopGroup loopGroup(parallelParam, offsetParam);

    Variable var(nullptr);
    auto assign = loopGroup.SetParallelParam(var);

    EXPECT_NE(assign, nullptr);
    EXPECT_EQ(assign->Type(), CcuRepType::ASSIGN);
}

TEST_F(CcuRepLoopGroupTest, SetOffsetParam)
{
    Variable parallelParam(nullptr);
    Variable offsetParam(nullptr);
    CcuRepLoopGroup loopGroup(parallelParam, offsetParam);

    Variable var(nullptr);
    auto assign = loopGroup.SetOffsetParam(var);

    EXPECT_NE(assign, nullptr);
    EXPECT_EQ(assign->Type(), CcuRepType::ASSIGN);
}

TEST_F(CcuRepLoopGroupTest, Translate)
{
    Variable parallelParam(nullptr);
    Variable offsetParam(nullptr);
    CcuRepLoopGroup loopGroup(parallelParam, offsetParam);

    CcuInstr instr {};
    CcuInstr* instrPtr = &instr;
    uint16_t instrId = 0;
    TransDep dep {0, 0, 1, 1, 0, {0, 0}, 0, 0, 0, {0, 0, 0}, {0, 0}, 0, false};

    bool result = loopGroup.Translate(instrPtr, instrId, dep);

    EXPECT_TRUE(result);
    EXPECT_EQ(instrId, 1U);
}

TEST_F(CcuRepLoopGroupTest, GetStartLoopInstrId)
{
    Variable parallelParam(nullptr);
    Variable offsetParam(nullptr);
    CcuRepLoopGroup loopGroup(parallelParam, offsetParam);

    CcuInstr instr {};
    CcuInstr* instrPtr = &instr;
    uint16_t instrId = 10;
    uint16_t originalInstrId = instrId;
    TransDep dep {0, 0, 1, 1, 0, {0, 0}, 0, 0, 0, {0, 0, 0}, {0, 0}, 0, false};

    loopGroup.Translate(instrPtr, instrId, dep);

    uint16_t startLoopInstrId = loopGroup.GetStartLoopInstrId();
    EXPECT_EQ(startLoopInstrId, originalInstrId + 3);
}

TEST_F(CcuRepLoopGroupTest, Describe)
{
    Variable parallelParam(nullptr);
    Variable offsetParam(nullptr);
    CcuRepLoopGroup loopGroup(parallelParam, offsetParam);

    std::string desc = loopGroup.Describe();
    EXPECT_NE(desc.find("LoopGroup"), std::string::npos);
}

class CcuRepSetLoopTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(CcuRepSetLoopTest, Constructor)
{
    Variable loopParam(nullptr);
    Executor executor(nullptr);
    Variable var(nullptr);
    CcuRepSetLoop setLoop(loopParam, executor, var);

    EXPECT_EQ(setLoop.Type(), CcuRepType::SET_LOOP);
    EXPECT_EQ(setLoop.loopParam.Id(), loopParam.Id());
    EXPECT_EQ(setLoop.executor.Id(), executor.Id());
    EXPECT_EQ(setLoop.var.Id(), var.Id());
}

TEST_F(CcuRepSetLoopTest, Translate)
{
    Variable loopParam(nullptr);
    Executor executor(nullptr);
    Variable var(nullptr);
    CcuRepSetLoop setLoop(loopParam, executor, var);

    CcuInstr instr[2] = {};
    CcuInstr* instrPtr = instr;
    uint16_t instrId = 0;
    TransDep dep {0, 0, 1, 1, 0, {0, 0}, 0, 0, 0, {0, 0, 0}, {0, 0}, 0, false};

    bool result = setLoop.Translate(instrPtr, instrId, dep);

    EXPECT_TRUE(result);
    EXPECT_EQ(instrId, 2U);
}

TEST_F(CcuRepSetLoopTest, Describe)
{
    Variable loopParam(nullptr);
    Executor executor(nullptr);
    Variable var(nullptr);
    CcuRepSetLoop setLoop(loopParam, executor, var);

    std::string desc = setLoop.Describe();
    EXPECT_NE(desc.find("loopParam"), std::string::npos);
    EXPECT_NE(desc.find("var"), std::string::npos);
}

}
}
}