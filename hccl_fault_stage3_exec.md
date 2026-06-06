# HCCL故障诊断 - 任务下发执行阶段详细指南

本文档提供任务下发执行阶段的详细故障诊断步骤和案例分析。

## 阶段概述

任务下发执行阶段关键任务：
- 通信算子任务编排及下发
- notify同步机制确保对端准备接收
- 集群心跳监控
- task exception机制处理异常

## 1. 集群心跳机制

### 原理说明

HCCL基于通信域信息与邻近rank建立维测链路，提供单点故障广播扩散能力，使任意rank的plog日志均包含故障根节点信息。

### 支持的故障探测能力

| 优先级 | 异常类型 | status（run/plog） | ExceptionType（debug/plog） | 判断标准 |
|--------|---------|-------------------|----------------------------|----------|
| 1 | 网络问题 | ERROR CQE | Error cqe Occurred | 定期轮询ROCE驱动重传超次事件，通过QPN映射远端IP |
| 2 | 进程卡死 | STUCK | Stuck Occurred | 每隔1/3 HCCL_EXEC_TIMEOUT轮询算子入/出次数分析是否卡住 |
| 3 | 进程退出 | LOST | Heartbeat Lost Occurred | 30s内未收到远端心跳报文 |

**注意**：ERROR日志只打印前3个有效事件（优先级：ERROR CQE > STUCK > LOST）

### 日志格式

#### 运行日志（run/plog）
```
[INFO] HCCL(686,python):2025-10-23-07:52:59.191.363 [heartbeat.cc:951] 
[8970][TaskExecStage][HeartbeatAbnormal]local rank [127.10.0.1/1]: 
crimer rank [127.10.0.2/2] status[LOST] by informer rank [127.10.0.3/3]
```

**字段说明**：
- HeartbeatAbnormal → 心跳异常事件
- local rank → 当前节点信息
- crimer rank → 根节点信息
- status → 异常事件类型
- by informer rank → 集群故障上报者

#### 调试日志（debug/plog）
```
[ERROR]HCCL(835695,all_reduce_test):2025-10-23-17:28:06.049.385 
[task_exception_handler.cc:610][835695][TaskExecStage][HeartbeatAbnormal]
Cluster Exception Location[IP/ID]:[127.10.0.1/1], 
Arrival Time:[Thu Oct 23 17:25:58 2025], 
Discoverer:[127.10.0.1/2], 
ExceptionType:[Heartbeat Lost Occurred], 
Possible Reason:1. Process has exited, 2. Network Disconnected
```

**字段说明**：
- Cluster Exception Location → 集群故障发生位置
- Arrival Time → 集群故障发生时间
- Discoverer → 集群故障发现节点
- ExceptionType → 集群故障异常类型
- Possible Reason → 可能原因及排查思路

### 定位思路

1. 在任意节点的CANN日志中检索心跳异常事件
2. 从异常事件日志中定位故障根节点
3. 根据异常类型排查具体原因

若超时后未伴随异常事件 → 可能为集群行为一致性问题，排查脚本、版本、数据集等

### 异常类型说明

#### Heartbeat Lost Occurred（进程退出）
**排查**：
- 异常节点是否提前退出
- 节点间网络是否异常无法连接

#### Stuck Occurred（进程卡死）
**排查**：
- 业务进程是否卡死在某个节点
- 是否发生死锁

#### Error cqe Occurred（网络丢包）
**排查**：
- 异常节点是否发生CQE error
- 网络是否异常

### 查询命令
```bash
grep -rE "HeartbeatAbnormal|Cluster Exception Location" run/plog/plog-*.log
```

## 2. task exception机制

### 原理说明

HCCL任务编排完成后下发到Device侧异步执行，若任务执行失败，回调函数通知HCCL异常task信息，HCCL打印失败task详细信息及算子信息。

**注意**：Atlas A3/A2产品需通过HCCL_DIAGNOSE_ENABLE手动开启task级诊断。

### 日志格式

```text
[ERROR] HCCL(2111667,all_reduce_test):2025-10-24-11:18:29.597.044 
[task_exception_handler.cc:908] [2111667][TaskExecStage][Timeout][Host]
Task run failed, base information is streamID:[2], taskID[21], 
tag[AllReduce_127.10.0.1%enp_60000_0_1761275812718970], 
AlgType(level 0-1-2):[fullmesh-ring-NHR].

[ERROR] HCCL(2111667,all_reduce_test):2025-10-24-11:18:29.597.054 
[task_exception_handler.cc:771] [2111667][TaskExecStage][Timeout][Host]
Task run failed, groupRank information is 
group:[127.10.0.1%enp_60000_0_1761275812718970], 
user define information[], rankSize[4], rankId[2].

[ERROR] HCCL(2111667,all_reduce_test):2025-10-24-11:18:29.597.083 
[task_exception_handler.cc:704] [2111667][TaskExecStage][Timeout][Host]
Task run failed, opData information is timeStamp:[2025-10-24-11:16:55.490.253], 
deviceId[2], index[21], count[256], reduceType[sum], 
src[0x12c0c0013000], dst[0x12c0c0014000], dataType[float32].
```

### 关键信息解读

**base information**：
- streamID、taskID → 任务位置
- tag → 算子标识符
- AlgType → 算法类型

**groupRank information**：
- group → 通信域名
- rankSize → 通信域大小
- rankId → 当前rank号

**opData information**：
- deviceId → 设备ID
- index → 该通信域下第几个算子
- count → 数据量
- reduceType → 规约类型
- src/dst → 输入输出地址
- dataType → 数据类型

### 常见失败task类型

- **Notify**：等待远端超时
- **SDMA**：HCCS链路异常、ECC错误、远端core dump

### 查询命令
```bash
grep -rE "Task run failed|TaskExecStage" debug/plog/plog-*.log
```

## 3. notify wait超时 (EI0002) - 重点

### 定位思路

#### 步骤1：确认通信域内全部rank节点位置

根据报错信息中的通信域名找到所有rank所在节点进程：

```bash
grep -r "Entry-" run/plog/ | grep "通信域名"
```

示例：
```bash
grep -r "Entry-" run/plog/ | grep "127.10.0.1%enp_60000_0_1761275812718970"
```

结果：
```text
run/plog/plog-2111667_*.log:[INFO] ...Entry-HcclCommInitRootInfoInner:
ranks[4], rank[2], rootinfo: host ip[127.10.0.1] ... 
identifier[127.10.0.1%enp_60000_0_1761275812718970], deviceLogicId[2]
```

#### 步骤2：确认通信域内其他rank行为

**情况1**：某个rank存在其他报错
- → 先排查对应rank报错原因

**情况2**：全部rank都是HCCL通信算子执行报错
- → 排查所有rank的算子、数据量、数据类型是否一致

示例分析：
```text
rank0: tag[AllReduce_***] ...  # AllReduce算子
rank1: tag[AllGather_***] ...  # AllGather算子（不一致）
```
→ 排查不同rank下发算子不一致根因

**情况3**：算子、数据量均一致，检查报错时间间隔

查看报错时间是否超过HCCL_EXEC_TIMEOUT（默认1836s）：
```bash
grep -r "HCCL_EXEC_TIMEOUT" run/plog
```

示例：
```text
rank0: 2025-10-24-22:03:14.*** Task run failed
rank1: 2025-10-24-22:08:58.*** Task run failed
```
→ 时间差5分40秒（340s），超时时间300s → 都超时
→ 排查算子下发时间间隔原因或调大HCCL_EXEC_TIMEOUT

#### 步骤3：确认通信算子下发行为是否异常

开启HCCL_ENTRY_LOG_ENABLE复现，查看算子下发入参：
```text
[INFO] HCCL(3015875,python):2025-03-07-11:43:32.305.623 
Entry-HcclAllReduce: tag[AllReduce_***], ..., streamId[7], ...

[INFO] HCCL(3015875,python):2025-03-07-11:43:32.306.413 
Entry-HcclAllReduce: tag[AllReduce_***], ..., streamId[2], ...
```

**问题**：同一通信域下两个AllReduce算子下发在不同stream（streamId[7]和[2]）
- → 多流并发执行，若未正确同步，资源被错误消耗
- → 导致执行超时或精度异常

### 查询命令
```bash
# 确认通信域rank
grep -r "Entry-" run/plog/ | grep "通信域名"

# 查看超时配置
grep -r "HCCL_EXEC_TIMEOUT" run/plog

# 查看算子下发记录（需开启HCCL_ENTRY_LOG_ENABLE）
grep -r "Entry-Hccl" run/plog/plog-*.log
```

## 4. SDMA ERROR (EI0012)

### 问题现象

打屏日志：
```text
[PID: 3480365] 2025-12-24-14:10:31.094.189 
Execution_Error_SDMA(EI0012): SDMA memory copy task exception occurred. 
Remote rank: [4800]. Base information: [streamID:[351], taskID[5], ...].
```

CANN日志：
```text
[ERROR] RUNTIME(57096,python3.10):2025-05-12-20:55:44.705.025 [task_info.cc:1170]
57288 PrintSdmaErrorInfoForFftsPlusTask:fftsplus task execute failed, 
dev_id=0, stream_id=50, task_id=21, context_id=18, thread_id=0, 
err_type=4[fftsplus sdma error]
```

### 可能原因

执行SDMA内存拷贝时页表转换失败：
- 输入或输出地址未分配内存
- 分配的内存小于拷贝大小
- 分配的内存已被释放

**常见根因**：

1. **通信域析构时机错误**
   - 下发算子后未执行流同步确认完成就析构通信域
   - HCCL Buffer地址被释放 → SDMA页表转换失败
   - 查询通信域销毁时间：`grep -r "Entry-HcclCommDestroy" run/plog`

2. **Atlas A3产品网络链路故障**
   - 检查两端链路状态

3. **内存大小不足**
   - 传入的输入/输出地址实际分配内存小于count数据量

### 解决方法

- 确保通信域析构前完成流同步
- 检查链路状态
- 确保内存分配足够

### 查询命令
```bash
grep -rE "EI0012|Execution_Error_SDMA|fftsplus sdma error" debug/plog/plog-*.log

# 查看通信域销毁时间
grep -r "Entry-HcclCommDestroy" run/plog
```

## 5. ERROR CQE报错 (EI0013)

### 问题现象

打屏日志：
```text
[PID: 3448331] 2025-12-04-21:59:08.232.310 
Execution Error ROCE CQE(EI0013): An error CQE occurred during operator execution. 
Local information: server 127.0.0.1, device ID 0, device IP 127.10.0.1. 
Peer information: server 127.10.0.2, device ID 1, device IP 127.10.0.2.
```

CANN日志：
```text
[ERROR] ROCE(2040034,alltoall_test):2025-09-15-08:38:12.776.612 
[hns_roce_lite.c:630]hns_roce_lite_handle_error_cqe(630) : error cqe status: 0x15

[ERROR] HCCL(2040034,alltoall_test):2025-09-15-08:38:13.607.432 
[heartbeat.cc:1229] [2040666][TaskExecStage][HeartbeatAbnormal][ROCE CQE ERROR]
cqe error status[12], time:[2025-09-15 08:38:12.776654], 
localInfo{server[127.10.0.1],deviceId[127.10.0.1],deviceIp[127.11.0.1]}, 
remoteIP{server[127.10.0.2],deviceId[127.10.0.2],deviceIp[127.11.0.2]}
```

### 可能原因

本端给对端发包后在指定时间内未收到确认回复：
- 网络通道异常
- 对端已断开连接或连接状态差无法响应
- 对端进程异常退出

### 定位步骤

#### 步骤1：确认ERROR CQE远端

从日志中获取localIP和remoteIP（本端和远端device ip）

#### 步骤2：排查网络问题

查询网口闪断记录：
```bash
hccn_tool -i 0 -link_stat -g
```

示例：
```text
[devid 0]current time        : Tue Oct 28 21:46:46 2025
[devid 0]link up count       : 2
[devid 0]link down count     : 1
[devid 0]link change records :
[devid 0]    Sun Oct  5 10:13:51 2025    LINK UP
[devid 0]    Sun Oct  5 10:13:50 2025    LINK DOWN  # 网口断链
[devid 0]    Sun Oct  5 10:13:35 2025    LINK UP
```
→ 网口在10:13:50断链 → ERROR CQE → 排查网口闪断原因

#### 步骤3：排查对端进程状态

检查对端在本端发生ERROR CQE前是否异常退出或进入资源销毁流程

#### 步骤4：调整RDMA参数

若HCCL_RDMA_TIMEOUT和HCCL_RDMA_RETRY_CNT较小，链路状态不佳时易出现ERROR CQE
- → 调大环境变量

**status[12]**表示RoCE报文重传超时，其他状态码极少见，需联系技术支持。

### 查询命令
```bash
grep -rE "EI0013|error cqe status|Error ROCE CQE" debug/plog/plog-*.log

# 查看网口状态
hccn_tool -i {device_id} -link_stat -g
```

## 6. AIV通信算子执行失败（Atlas A2产品）

### 问题现象

开启HCCL_OP_EXPANSION_MODE="AIV"后，HCCL以kernel方式执行通信算子：

```text
[ERROR] RUNTIME(699131,python):2025-04-24-21:54:17.707.236 
[davinci_kernel_task.cc:1268]699131 PrintErrorInfoForDavinciTask:
[INIT][DEFAULT]Aicore kernel execute failed, device_id=0, stream_id=2, 
task_id=55873, fault kernel_name=aiv_all_reduce_***, 
fault kernel info ext=aiv_all_reduce_910b_bfloat16_t, ...
```

### 定位思路

按notify wait超时（EI0002）排查思路分析：
- 是否全量超时
- 集群中是否存在某个先发生异常的节点

## 7. 检查清单

### 集群心跳检查
- [ ] 检查心跳异常事件日志
- [ ] 定位集群故障根节点
- [ ] 根据异常类型排查（LOST/STUCK/Error CQE）
- [ ] 检查进程状态和core dump情况

### task exception检查
- [ ] 检查task exception信息（streamID、taskID、tag）
- [ ] 确认通信域内全部rank位置
- [ ] 检查其他rank是否有报错
- [ ] 检查算子下发行为一致性
- [ ] 检查报错时间间隔是否超过HCCL_EXEC_TIMEOUT

### notify wait检查
- [ ] 检查算子类型是否一致
- [ ] 检查数据量是否一致
- [ ] 检查数据类型是否一致
- [ ] 检查streamId是否一致（多流场景）
- [ ] 检查下发时间间隔

### SDMA ERROR检查
- [ ] 检查通信域析构时机是否正确
- [ ] 检查链路状态（Atlas A3）
- [ ] 检查内存分配是否足够

### ERROR CQE检查
- [ ] 确认ERROR CQE远端信息
- [ ] 查看网口闪断记录
- [ ] 检查对端进程状态
- [ ] 调整RDMA参数（HCCL_RDMA_TIMEOUT、HCCL_RDMA_RETRY_CNT）

---

**相关命令**：
- `/skill hccl_fault_diagnosis_main` - 返回主诊断指南
- `/skill hccl_fault_stage1_init` - 通信域初始化阶段诊断
- `/skill hccl_fault_stage2_link` - 参数面建链阶段诊断