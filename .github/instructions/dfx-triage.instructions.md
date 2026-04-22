---
applyTo: "src/framework/cluster_maintenance/**,src/framework/next/coll_comms/dfx/**,src/legacy/framework/dfx/**,src/framework/communicator/impl/**"
description: "DFX 故障定位与异常排查说明（task exception / profiling / connection anomaly）"
---

# DFX triage 指引

适用于本仓库中与 DFX 故障定位、task exception、profiling、connection anomaly 相关的代码修改与排查。

## 1. 先看文档真源，不要重复写流程

开始排查前，优先阅读并引用以下文档：

- [`docs/dfx_task_exception_flow.md`](../../docs/dfx_task_exception_flow.md) — task exception 主流程、legacy/next 路径、panic log 入口。
- [`docs/dfx_profiling_flow.md`](../../docs/dfx_profiling_flow.md) — CCU profiling 的构造期/翻译期/运行时绑定链路。

若用户请求的是“理解/定位/修复”现有 DFX 行为：
- **链接到上述文档即可**，不要把整段流程复制进说明文件或代码注释。
- 只有在代码行为发生变化、旧文档不再准确时，才补充或更新文档。

## 2. task exception 排查入口

### 2.1 next 路径优先

新框架优先查看：

- `src/framework/next/coll_comms/dfx/taskException/host/ccuTaskException.{h,cc}`
- `src/framework/next/coll_comms/dfx/taskException/aicpu/**`

排查时先确认异常属于哪类：
- **CCU task exception**：优先从 `ccuTaskException` 入口追。
- **AICPU task exception**：优先从 `hcclCommTaskExceptionLite.cc`、`AicpuIndopProcess` 和 `CollCommAicpuMgr/CollCommAicpu` 的关联关系追。
- **legacy task exception**：只在问题明确落在旧流程时进入 `src/legacy/framework/dfx/task_exception/**`。

### 2.2 关注“数据来源”而不是只看打印

排查异常上报问题时，先确认：
- 异常信息来自 **RTS/CQE/panic log/flagMem/GlobalMirrorTasks** 中的哪一类。
- 错误码转换是否发生在 host 侧还是 device/aicpu 侧。
- 打印函数是否只是消费数据，还是同时承担状态清理、上报、去重逻辑。

不要只改打印文本而不核对数据来源与上报路径。

## 3. profiling 排查入口

CCU profiling 相关修改，优先对照以下文件：

- `pkg_inc/hcomm/ccu/ccu_rep_context_v1.h`
- `src/framework/next/comms/ccu/ccu_kernel/ccu_kernel.cc`
- `src/framework/next/comms/ccu/ccu_kernel/ccu_kernel_mgr.cc`
- `src/framework/next/coll_comms/api_c_adpt/coll_comm_res_c_adpt.cc`

排查原则：
- 明确当前问题属于 **构造期占位**、**翻译期注入 instrId**、还是 **运行时绑定 taskArgs / dataSize**。
- 不要把 profiling 缓存字段误当成“纯性能优化”；它们主要用于跨阶段保留语义。
- 修改上报字段前，先确认字段是在 kernel 内部补全，还是在 `HcclReportCcuProfilingInfo` 阶段补全。

## 4. connection anomaly / 故障节点探测入口

连接异常探测优先查看：

- `src/framework/cluster_maintenance/detect/detect_connect_anomalies/**`
- `src/framework/communicator/impl/resource_manager/transport_manager.cc`
- `src/framework/communicator/impl/hccl_communicator_host.cc`
- `src/framework/common/src/config/env_config.{h,cc}`

排查时先回答三个问题：
1. **谁触发 AddIpQueue**：是 P2P enable 失败、socket 建链失败还是其他 transport 错误路径。
2. **功能是否启用**：以 `GetExternalInputDfsConnectionFaultDetectionTime()` 为准。
3. **是否受设备类型限制**：当前仅支持特定 device type（例如 `910_93` / `910B`）。

重要约束：
- 这个能力当前不是独立布尔开关；其启停由 `HCCL_DFS_CONFIG` 中的 `connection_fault_detection_time` 控制。
- 若 `connection_fault_detection_time == 0`，视为关闭探测。
- 修改探测逻辑时，必须同时核对超时等待、线程生命周期和白名单清理逻辑，避免只改触发条件造成资源泄漏或刷屏。

## 5. 做新 DFX 能力时的建议

如果本次任务属于“DFX 要准备新做的或复用的迭代功能”，推荐复用这份 instruction；因为这类任务通常需要：
- 同时理解 docs 中的现有流程；
- 在 `next/dfx`、`cluster_maintenance`、`communicator/impl` 之间来回追调用链；
- 区分“已有行为排查”与“新增观测点/新增上报字段”。

进行这类变更时：
- **优先复用现有上报链路和字段结构**，避免平行再造一套 DFX 通道。
- 若新增 feature 开关，优先与现有 `HCCL_DFS_CONFIG` / env_config 体系保持一致。
- 若只是为定位问题补充日志，优先落在已有 DFX 入口，避免把业务逻辑和调试逻辑混在一起。

## 6. 提交前最少自检

涉及 DFX triage 的改动，提交前至少确认：
- 文档链接仍然准确，未与代码行为脱节。
- legacy / next 路径没有混用错误的数据结构或命名风格。
- host/device/aicpu 侧错误码与状态转换链路一致。
- connection anomaly 修改后不会改变默认启停语义，除非任务明确要求。
