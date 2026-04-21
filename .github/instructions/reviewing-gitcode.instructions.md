---
applyTo: "**/*.{h,hpp,c,cc,cpp,cxx,inc,py,cmake,txt,md}"
description: "HCOMM 风险导向代码检视指引（参考 reviewing-gitcode）"
---

# HCOMM 代码检视指引

适用于本仓库中的代码 review、PR 检视、变更风险分析。

## 1. 检视目标

当用户请求 review、检视 PR、分析当前改动风险时，优先关注：

- 正确性错误
- 行为回归
- 资源泄漏或生命周期问题
- 并发与同步问题
- host/device 路径不一致
- 接口变更未同步调用方
- 缺失的测试或测试覆盖不足

不要把主要精力放在纯格式、纯命名或低价值风格意见上，除非它们已经影响可读性、接口一致性或正确性。

## 2. 输出顺序

review 结论必须按下面顺序组织：

1. Findings：按严重级别排序，先高后低
2. Open questions / assumptions：仅列真正影响结论的前提
3. Brief summary：可选，保持简短

若无明确问题，直接说明“未发现明确缺陷”，并补一句剩余风险或测试缺口。

## 3. HCOMM 特有高风险点

### 3.1 资源与回滚

涉及以下路径时，优先检查失败路径和回滚完整性：

- `src/framework/communicator/impl/**`
- `src/framework/next/comms/**`
- `src/framework/next/coll_comms/**`
- `src/framework/cluster_maintenance/**`

重点看：

- 批量创建/批量销毁是否半成功半失败
- 异常返回前是否释放已申请资源
- 新增空指针/错误码分支是否改变原语义
- handle、jetty、memory、transport、channel 是否存在遗漏清理

### 3.2 DFX / task exception / profiling

涉及 DFX 代码时，不要只看打印文本，必须核对：

- 数据来源是否正确
- 判型逻辑是否单点收敛
- legacy / next 是否混用不同 schema 或结构
- 新增字段是否与文档和调用点一致

优先参照：

- `docs/dfx_task_exception_flow.md`
- `docs/dfx_profiling_flow.md`
- `.github/instructions/dfx-triage.instructions.md`

### 3.3 公共接口与兼容性

涉及 `pkg_inc/`、`include/`、`docs/comm_mgr_api_c/`、`docs/comm_opdev_api/` 时，优先检查：

- ABI / API 是否发生变化
- 调用方是否同步更新
- 文档是否仍然准确
- host/device 两侧结构体布局是否仍兼容

## 4. 检视方法

### 4.1 先看变更，再补上下文

优先基于当前提交或当前 diff 做 review，不要先全仓漫游。

最少补充以下上下文：

- 被改函数/类型的定义
- 直接调用方或被调用方
- 同目录下相似实现
- 相关测试

### 4.2 用“行为问题”写 finding

finding 应写成“什么条件下会出什么问题，为什么”，不要只写“建议优化”。

推荐结构：

- 问题位置
- 触发条件
- 实际风险
- 为什么当前代码会导致该风险

### 4.3 测试是 review 的一部分

检视时必须判断：

- 是否已有测试覆盖变更行为
- 变更是否需要补 UT / ST
- 测试是否只覆盖主路径、未覆盖失败路径

## 5. 不建议的 review 方式

- 不要把整份 review 写成 changelog
- 不要只复述代码做了什么
- 不要把“可以更优雅”当成主要问题
- 不要在缺少证据时臆测运行时行为
- 不要忽略失败路径、回滚路径和兼容性
