# HCCL故障诊断 - 参数面建链阶段详细指南

本文档提供参数面建链阶段的详细故障诊断步骤和案例分析。

## 阶段概述

参数面建链阶段关键任务：
- HCCL通过参数面网络基于TCP协议进行socket连接创建
- 交换算子入参、CANN版本等信息
- 进行参数一致性校验

## 1. 建链失败定位思路

### 建链根节点定位机制

**原理说明**

HCCL会在建链失败后启动故障探测链路：

1. 每个rank建链失败后启动server端监听所有rank探测链路
2. 向无法响应业务建链请求的远端发起探测连接
3. 若远端无法响应探测 → 产生探测失败事件并扩散
4. 若远端建立探测链路 → 接收并转发探测失败事件

**优势**：快速定位故障点的节点位置

### 一致性校验机制

HCCL与对端建立socket后会校验：
- 算子入参
- CANN版本
- 其他关键信息

**注意**：单算子模式下，仅首次调用新类型算子触发建链，无法拦截所有下发不一致问题。

## 2. 参数面端口绑定失败 (EI0019)

### 问题现象

```text
[ERROR] HCCL(1009464,all_reduce_test):2025-03-15-00:41:48.470.172 [hccl_socket.cc:110] 
[1009464][InitGroupStage][RanktableDetect] socket type[0], 
listen on ip[192.168.2.199] and specific port[16666] fail. 
Please check the port status and whether the port is being used by other process.
```

**关键信息**
- socket type[0或1] → Device侧端口绑定失败（参数面建链）
- socket type[2] → Host侧端口绑定失败（集群协商）

### 可能原因

当前rank在参数面建链时需绑定device侧网卡端口（默认16666），但端口已被其他进程占用。

### 解决方法

```bash
# 多进程场景配置端口范围
export HCCL_NPU_SOCKET_PORT_RANGE="auto"
```

排查多个进程是否符合预期，若不符合需调整业务配置。

### 查询命令
```bash
grep -rE "socket type\[(0|1)\].*Please check the port" debug/plog/plog-*.log
```

## 3. QP内存资源申请失败 (EI0011)

### 问题现象

```text
[PID: 2103452] 2025-11-03-20:18:46.447.213 
Resource_Error_Insufficient_Device_Memory(EI0011): 
Failed to allocate [size: [0.25MB, 3MB]] bytes of NPU memory.
Possible Cause: Allocation failure due to insufficient NPU memory.
```

### 解决方法

1. 调整业务配置（如batchSize）
2. 减少ROCE链路使用数量
3. 释放部分device侧内存
4. 配置HCCL_BUFFSIZE调整内存大小

**注意**：HCCL其他内存OOM由drv组件上报，可通过HCCL_BUFFSIZE调整。

### 查询命令
```bash
grep -rE "EI0011|Resource_Error_Insufficient_Device_Memory" debug/plog/plog-*.log
```

## 4. 建链超时 (EI0006) - 重点

### 问题现象

```text
[ERROR] HCCL(17528,python3):2026-03-18-10:33:52.113.403 [hccl_socket_manager.cc:797] 
[18744][Wait][LinkEstablish]wait socket establish timeout, role[1] rank[1] timeout[120 s]

[ERROR] HCCL(17528,python3):2026-03-18-10:33:52.113.646 [hccl_socket_manager.cc:623] 
[18744] _________________________LINK_ERROR_INFO___________________________
[ERROR] HCCL(17528,python3):2026-03-18-10:33:52.113.653 [hccl_socket_manager.cc:626] 
[18744] | dest_ip(user_rank) | dest_port | src_ip(user_rank) | src_port | MyRole | Status | TlsStatus |
[ERROR] HCCL(17528,python3):2026-03-18-10:33:52.113.706 [hccl_socket_manager.cc:583] 
[18744] | 192.0.2.199(0) | 16666 | 192.0.3.198(1) | 3234403008 | client | time out | DISABLE | LinkInfo

[ERROR] HCCL(17528,python3):2026-03-18-10:34:43.142.013 [transport_manager.cc:1325] 
[18744][InitChannelStage][Timeout]Transport init error! createLink para:
rank[1]-localUserrank[1]-localIpAddr[192.168.200.100/1], 
remoteRank[0]-remoteUserrank[0]-remoteIpAddr[192.168.200.100/0], 
machineType[1], linkMode[1], isUsedRdma[0], tag[HcomAllReduce_***]
```

### 关键信息解读

**LINK_ERROR_INFO表格**：
- dest_ip(user_rank) → 对端device ip和rank号
- src_ip(user_rank) → 本端device ip和rank号
- Status → 链路状态（time out）
- TlsStatus → TLS状态（UNKNOWN/DISABLE/ENABLE）

**Transport init error信息**：
- localUserrank → 本端rank号
- localIpAddr → 本端节点IP[hostIp/deviceId]
- remoteUserrank → 对端rank号
- remoteIpAddr → 对端节点IP
- tag → 通信算子标识符

### 定位步骤

#### 步骤1：确认建链失败对端

**方法1**：查看探测事件列表（DETECT EVENT LIST）

**方法2**：查看LINK_ERROR_INFO表格 + Transport init error信息

```bash
grep -r "Transport init error! createLink para:" debug/plog/plog-*.log
```

#### 步骤2：检查对端行为（5个排查点）

**排查点1**：对端无报错日志
- → 对端未同步下发算子
- → 排查两端算子下发行为一致性

**排查点2**：对端有其他报错
- → 先排查对端其他报错原因

**排查点3**：对端也建链超时但对象不同
- → 按流程先排查对端建链超时原因

**排查点4**：对端也和本端建链超时，检查报错时间
- → 查看两端报错时间是否超过HCCL_CONNECT_TIMEOUT
- → 若超过：排查算子下发时间间隔原因
- → 查看超时配置：`grep -r "HCCL_CONNECT_TIMEOUT" run/plog`

**排查点5**：对端和本端都在超时时间内建链超时，检查网络

##### 检查TLS开关一致性

**方法1**：查看LINK_ERROR_INFO表格的TlsStatus

**方法2**：查询日志
```bash
grep -r "TLS SWITCH" run/device-*/*.log
```

**方法3**：使用hccn_tool
```bash
hccn_tool -i {device_id} -tls -g
```

##### 检查网络连通性

```bash
hccn_tool -i {本地device_id} -ping -g address {对端device_ip}
```

若ping不通或网口down，联系管理员排查网卡和交换机配置。

##### 检查超节点配置（Atlas A3产品）

**典型错误**：不同物理超节点配置为同一逻辑超节点

本端日志：
```text
[ERROR] local rank information: nicType[VNIC_TYPE], 
logicSuperPodId[logic_1], phySuperPodId[0]. 
Note: Do not configure ranks belonging to different physical superpod ID 
info a single logical superpod ID
```

远端日志：
```text
[ERROR] local rank information: nicType[VNIC_TYPE], 
logicSuperPodId[logic_1], phySuperPodId[1]. 
Note: Do not configure ranks belonging to different physical superpod ID 
info a single logical superpod ID
```

**解决方法**：修改或取消HCCL_LOGIC_SUPERPOD_ID配置

### 非全量建链超时快速判断

若非全量建链超时，重点排查未报错的节点：
```bash
for i in *;do cd $i;pwd;grep -rnc "connection fail" | grep -v ":0" | wc -l; cd ..;done
```

### 行为一致性问题

若探测无任何事件 → 可能为行为一致性问题

**特征**：每个rank均进入建链阶段并响应探测，但算子不一致导致互等超时

**排查方法**：
- 检查脚本、环境、版本、数据集
- 遍历每个rank的tag信息分析算子行为差异
- 开启HCCL_ENTRY_LOG_ENABLE跟踪算子行为

### 注意事项

1. 探测失败事件阈值默认20s，可通过HCCL_DFS_CONFIG调整
2. 复杂场景需多次跳转才能定位根节点
3. 根节点特征：其他报错、无异常日志、和其他rank互等超时

### 查询命令
```bash
# 建链超时
grep -r "wait socket establish timeout" debug/plog/plog-*.log

# 建链失败对端信息
grep -r "Transport init error! createLink para:" debug/plog/plog-*.log

# 探测事件
grep -r "DETECT EVENT" debug/plog/plog-*.log

# 查看超时配置
grep -r "HCCL_CONNECT_TIMEOUT" run/plog
```

## 5. 参数一致性校验失败 (EI0005)

### 问题现象

打屏日志：
```text
EI0005: 2024-04-24-06:32:27.781.599 
The arguments for collective communication are inconsistent between ranks:
parameter count, local end 16512, remote end 8320
```

CANN日志：
```text
[ERROR] HCCL(3743927,all_reduce_test):2025-10-25-16:11:16.831.640 
[rank_consistentcy_checker.cc:429] [3743951][InitChannelStage][ParameterConflict]
CMD information tag check fail. 
local[AllGather_127.10.0.1%enp_***], remote[AllReduce_127.10.0.1%enp_***]

[ERROR] HCCL(3743927,all_reduce_test):2025-10-25-16:11:16.831.666 
[rank_consistentcy_checker.cc:439] [3743951][InitChannelStage][ParameterConflict]
CMD information cmdType check fail. local[6], remote[2]

[ERROR] HCCL(3743927,all_reduce_test):2025-10-25-16:11:16.831.679 
[rank_consistentcy_checker.cc:439] [3743951][InitChannelStage][ParameterConflict]
CMD information op check fail. local[255], remote[0]
```

### 校验内容

参数面建链后校验：
- tag（算子标识符）
- cmdType（算子类型）
- op（规约类型）
- count（数据量）
- cclbufferSize（Buffer大小）
- dataType（数据类型）

### 解决方法

1. **SuperKernel场景**
   - 若启用SuperKernel后出现不一致
   - 将HCCL算子移出SuperKernel标定范围
   - 参考《PyTorch图模式使用指南》"图内标定SuperKernel范围"章节

2. **参数不一致**
   - 根据报错信息排查两端算子参数差异
   - 从业务上排查算子不一致根因

### cmdType和op枚举值

**cmdType（算子类型）**：
| 值 | 类型 | 值 | 类型 |
|---|------|---|------|
| 1 | BroadCast | 9 | AlltoAllVC |
| 2 | AllReduce | 10 | AlltoAll |
| 3 | Reduce | 11 | Gather |
| 4 | Send | 12 | Scatter |
| 5 | Receive | 13 | BatchSendRecv |
| 6 | AllGather | 16 | AllGatherV |
| 7 | ReduceScatter | 17 | ReduceScatterV |
| 8 | AlltoAllV | | |

**op（规约类型）**：
| 值 | 类型 |
|---|------|
| 0 | SUM |
| 1 | PROD |
| 2 | MAX |
| 3 | MIN |
| 255 | 非Reduce算子 |

### 查询命令
```bash
grep -r "CMD information.*check fail" debug/plog/plog-*.log
```

## 6. 检查清单

### 端口绑定检查
- [ ] 检查socket type判断是Host还是Device侧
- [ ] 检查端口是否被占用（16666默认端口）
- [ ] 多进程场景配置HCCL_NPU_SOCKET_PORT_RANGE

### QP内存检查
- [ ] 检查device侧内存是否充足
- [ ] 调整batchSize等业务配置
- [ ] 配置HCCL_BUFFSIZE调整内存大小

### 建链超时检查
- [ ] 确认建链失败对端（DETECT EVENT或LINK_ERROR_INFO）
- [ ] 检查对端是否有报错
- [ ] 检查两端报错时间间隔
- [ ] 检查TLS开关一致性
- [ ] 检查网络连通性（hccn_tool ping）
- [ ] 检查超节点配置（phySuperPodId）
- [ ] 检查算子行为一致性（tag信息）

### 参数一致性检查
- [ ] 检查tag是否一致
- [ ] 检查cmdType是否一致
- [ ] 检查op是否一致
- [ ] 检查count是否一致
- [ ] 检查dataType是否一致
- [ ] SuperKernel场景需特殊处理

---

**相关命令**：
- `/skill hccl_fault_diagnosis_main` - 返回主诊断指南
- `/skill hccl_fault_stage1_init` - 通信域初始化阶段诊断
- `/skill hccl_fault_stage3_exec` - 任务下发执行阶段诊断