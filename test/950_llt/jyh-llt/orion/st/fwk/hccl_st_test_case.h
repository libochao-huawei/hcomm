#ifndef HCCLV2_ST_TEST_CASE_H
#define HCCLV2_ST_TEST_CASE_H

#include "types.h"
#include "op_type.h"
#include "data_type.h"
#include "reduce_op.h"
#include "dev_type.h"
#include "env_config.h"
#include "hccl_st_situation.h"
#include <utility>
#include <vector>
#include <mutex>
#include "context/st_ctx.h"

void PrepareCtx(ThreadContext *ctx);
bool VerifyCtx(ThreadContext *ctx);

class HcclStTestCase {
public:
    explicit HcclStTestCase(Situation &situation, string caseName = "")
        : situation(situation), testcaseName(std::move(caseName))
    {
        InitSituationEnv();
    }

    void Start();
    bool Verify();

private:
    Situation situation;

    const string testcaseName;

    void InitSituationEnv();

    void InternalProcess(ThreadContext *ctx);

    void SetEnv();

    void UnsetEnv();

    vector<ThreadContext *> contexts;
};

#endif