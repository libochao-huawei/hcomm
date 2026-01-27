#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mockcpp/mokc.h>

#include "ccu_rep_translator.h"
#include "ccu_rep.h"

using namespace Hccl;
using namespace CcuRep;

class CcuRepLoadTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CcuRepLoadTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "CcuRepLoadTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in CcuRepLoadTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in CcuRepLoadTest TearDown" << std::endl;
    }
};

constexpr uint16_t INSTR_NUM = 7;
constexpr uint16_t XN_NUM = 4;
constexpr uint16_t GSA_NUM = 3;

void InitTransDep(TransDep &transDep)
{
    for (int i = 0; i < XN_NUM - 1; i++) {
        transDep.commXn[i] = i;
    }
    for (int i = 0; i < GSA_NUM - 1; i++) {
        transDep.commGsa[i] = i;
    }
    transDep.commSignal = 0;
    transDep.xnBaseAddr = 0x10000;
}

TEST_F(CcuRepLoadTest, rep_load_translate)
{
    uint64_t hbmAddr = 0x12341234;
    Variable var;
    var.Reset(0x123);
    CcuRepLoad ccuRepLoad(hbmAddr, var);

    EXPECT_EQ(ccuRepLoad.Type(), CcuRepType::LOAD);

    // 资源初始化
    size_t instrSize = sizeof(CcuInstr) * INSTR_NUM;
    CcuInstr* instr = (CcuInstr*)malloc(instrSize);
    memset_s(instr, instrSize, 0, instrSize);
    TransDep transDep{0};
    InitTransDep(transDep);

    // 翻译LOAD
    uint16_t instrId = 0;
    CcuInstr* instrOri = instr;
    bool translated = ccuRepLoad.Translate(instr, instrId, transDep);
    EXPECT_TRUE(translated);
    EXPECT_EQ(instrId, INSTR_NUM); // LOAD包含指令数
    EXPECT_EQ(ccuRepLoad.addr, 305402420);
    EXPECT_EQ(ccuRepLoad.var.Id(), 291);
    EXPECT_EQ(ccuRepLoad.num, 1);

    free(instrOri);
}

TEST_F(CcuRepLoadTest, Ut_CcuRepLoadVar_Translate_When_NormalInput_Expect_Success)
{
    Variable var0;
    var0.Reset(0x123);
    Variable var1;
    var1.Reset(0x456);
    CcuRepLoadVar ccuRepLoadVar(var0, var1);

    EXPECT_EQ(ccuRepLoadVar.Type(), CcuRepType::LOAD_VAR);

    // 资源初始化
    size_t instrSize = sizeof(CcuInstr) * INSTR_NUM;
    CcuInstr* instr = (CcuInstr*)malloc(instrSize);
    memset_s(instr, instrSize, 0, instrSize);
    TransDep transDep{0};
    InitTransDep(transDep);

    // 翻译LOADVAR
    uint16_t instrId = 0;
    CcuInstr* instrOri = instr;
    bool translated = ccuRepLoadVar.Translate(instr, instrId, transDep);
    EXPECT_TRUE(translated);
    EXPECT_EQ(instrId, INSTR_NUM);
    EXPECT_EQ(ccuRepLoadVar.src.Id(), 291);
    EXPECT_EQ(ccuRepLoadVar.var.Id(), 1110);

    free(instrOri);
}