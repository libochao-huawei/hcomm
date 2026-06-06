# HCCL故障诊断主Skill

基于CANN HCCL官方文档和实战经验，用于快速定位HCCL集合通信库故障。

## 1. 快速诊断流程

### 三步快速定位
```
1. 确认故障码 → 查故障码速查表
2. 收集日志 → 收集所有节点CANN日志（debug+run目录）
3. 确认阶段 → 根据多级检索关键字判断
```

### HCCL业务三阶段
- **通信域初始化阶段** - 环境变量配置、rank table加载、集群信息协商
- **参数面建链阶段** - socket连接创建、参数一致性校验
- **任务下发执行阶段** - 通信算子执行、集群心跳监控

## 2. 故障码速查表

| 故障码 | 故障说明 | 常见原因 | 阶段 |
|--------|---------|----------|------|
| EI0001 | 环境变量配置异常 | HCCL_RDMA_SL、HCCL_SOCKET_IFNAME配置错误 | 初始化 |
| EI0002 | 通信算子执行超时 | notify wait超时、对端未响应 | 执行 |
| EI0003 | 集合通信算子入参校验失败 | count、dataType等参数不一致 | 执行 |
| EI0004 | rankTable文件加载失败 | 文件路径不存在、权限不足、格式错误 | 初始化 |
| EI0005 | 参数一致性校验失败 | tag、cmdType、op、count等参数不一致 | 建链 |
| EI0006 | 通信算子参数面建链超时 | socket连接超时、网络连通性问题 | 建链 |
| EI0007 | 资源初始化失败 | 内存、端口等资源不足 | 初始化 |
| EI0008 | HCCL版本不一致校验失败 | 集群CANN版本不一致 | 建链 |
| EI0011 | QP内存资源申请失败 | Device侧内存不足(OOM) | 建链 |
| EI0012 | SDMA任务异常 | HCCS链路异常、ECC错误、远端core dump | 执行 |
| EI0013 | ROCE CQE ERROR异常 | 网络丢包、重传超次 | 执行 |
| EI0014 | 集群信息校验失败 | IP重复、superDeviceId重复、device_ip不一致 | 初始化 |
| EI0015 | 通信域集群信息协商阶段超时 | 部分rank未连接到server节点 | 初始化 |
| EI0016 | TLS信息配置不一致 | 通信域内TLS开关状态不一致 | 初始化 |
| EI0019 | 端口绑定失败 | Host侧或Device侧端口被占用 | 初始化/建链 |
| EI9999 | 未知故障或HCCL内部问题 | 较为罕见的故障场景 | - |

## 3. 多级检索关键字速查

### 通信域初始化阶段 (InitGroupStage)
- **EnvConfig** - 环境变量配置异常 → 查环境变量配置
- **RanktableConfig** - rankTable文件读取失败 → 查文件路径权限
- **RanktableCheck** - rankTable集群信息校验失败 → 查IP/TLS/superDeviceId
- **RanktableDetect** - 集群信息探测失败 → 查网络连通性
- **Resource** - 资源初始化失败 → 查内存端口

### 参数面建链阶段 (InitChannelStage)
- **ParameterConflict** - 参数一致性校验失败 → 查算子参数一致性
- **VersionConflict** - HCCL版本不一致校验失败 → 查CANN版本
- **Timeout** - 建链超时报错 → 查网络连通性、对端行为

### 任务下发执行阶段 (TaskExecStage)
- **InvalidArgument** - 算子入参校验失败 → 查算子参数
- **NotSupported** - 不支持场景 → 查算子类型
- **Timeout** - 执行超时 → 查心跳异常、对端行为
- **RunFailed** - 执行失败 → 查task exception信息
- **HeartbeatAbnormal** - 心跳异常事件 → 查集群故障根节点

## 4. 一键诊断命令集

### 识别故障阶段
```bash
# 查看故障码
grep -rE "EI[0-9]{4}|EJ[0-9]{4}" debug/plog/plog-*.log

# 查看故障阶段关键字
grep -rE "InitGroupStage|InitChannelStage|TaskExecStage" debug/plog/plog-*.log
```

### 通信域初始化阶段
```bash
# 环境变量配置
grep -r "HCCL_ENV" run/plog/plog-*.log

# rank table文件问题
grep -rE "is not a valid real path|RanktableCheck" debug/plog/plog-*.log

# 集群协商问题
grep -rE "topo exchange.*timeout|socket type\[2\]" debug/plog/plog-*.log

# 集群信息校验
grep -rE "device ip family|tls.*invalid|superDeviceId.*already exist" debug/plog/plog-*.log
```

### 参数面建链阶段
```bash
# 建链超时
grep -r "wait socket establish timeout" debug/plog/plog-*.log

# 建链失败对端信息
grep -r "Transport init error! createLink para:" debug/plog/plog-*.log

# 探测事件列表
grep -r "DETECT EVENT" debug/plog/plog-*.log

# 参数一致性校验
grep -r "CMD information.*check fail" debug/plog/plog-*.log

# 端口绑定失败
grep -rE "socket type\[(0|1)\].*Please check the port" debug/plog/plog-*.log

# QP内存申请失败
grep -rE "EI0011|Resource_Error_Insufficient_Device_Memory" debug/plog/plog-*.log
```

### 任务下发执行阶段
```bash
# 心跳异常
grep -rE "HeartbeatAbnormal|Cluster Exception Location" run/plog/plog-*.log

# task exception
grep -rE "Task run failed|TaskExecStage" debug/plog/plog-*.log

# SDMA ERROR
grep -rE "EI0012|Execution_Error_SDMA|fftsplus sdma error" debug/plog/plog-*.log

# ERROR CQE
grep -rE "EI0013|error cqe status|Error ROCE CQE" debug/plog/plog-*.log
```

### 集群信息查询
```bash
# 通信域信息
grep -r "Communicator Key Info" run/plog/plog-*.log

# 本端信息
grep -r "LocalRank Key Info" run/plog/plog-*.log

# 通信域创建下发记录
grep -r "Entry-HcclCommInitRootInfoInner" run/plog/plog-*.log

# 算子下发记录（需开启HCCL_ENTRY_LOG_ENABLE）
grep -r "Entry-Hccl" run/plog/plog-*.log
```

### 网络连通性检查
```bash
# 测试device侧连通性
hccn_tool -i {本地device_id} -ping -g address {对端device_ip}

# 查看TLS状态
hccn_tool -i {device_id} -tls -g

# 查看网口状态
hccn_tool -i {device_id} -link_stat -g

# 查看IP配置
hccn_tool -i {device_id} -ip -g
```

## 5. 关键环境变量速查

### 故障诊断相关
- **HCCL_CONNECT_TIMEOUT** - 建链超时时间（默认120s）
- **HCCL_EXEC_TIMEOUT** - 执行超时时间（默认1836s）
- **HCCL_ENTRY_LOG_ENABLE** - 算子级入参记录开关（0/1）
- **HCCL_DEBUG_CONFIG** - 模块级日志开关
- **HCCL_DIAGNOSE_ENABLE** - task级诊断信息开关（A3/A2需开启）

### 网络配置相关
- **HCCL_IF_IP** - Host侧网卡IP
- **HCCL_IF_BASE_PORT** - 基础端口（默认60000）
- **HCCL_SOCKET_IFNAME** - Host网卡名称
- **HCCL_RDMA_SL** - RDMA Service Level（范围[0,7]）
- **HCCL_SOCKET_FAMILY** - IP协议类型（2=IPv4, 10=IPv6）

### 多进程场景
- **HCCL_HOST_SOCKET_PORT_RANGE** - Host侧端口范围（配置"auto"）
- **HCCL_NPU_SOCKET_PORT_RANGE** - Device侧端口范围（配置"auto"）

## 6. 常见故障场景速查

### 全量建链超时
**现象**：所有rank都报建链超时  
**排查**：网络连通性、环境变量一致性、rank table一致性

### 非全量建链超时
**现象**：部分rank建链超时  
**排查**：重点排查未报错的节点  
**命令**：`for i in *;do cd $i;pwd;grep -rnc "connection fail" | grep -v ":0" | wc -l; cd ..;done`

### 行为一致性问题
**现象**：无明确首报错，多rank互等超时  
**排查**：
1. 检查脚本、环境、版本、数据集
2. 开启HCCL_ENTRY_LOG_ENABLE跟踪算子行为
3. 比对各rank的tag信息，分析算子调用差异

### 端口冲突
**现象**：EI0019端口绑定失败  
**解决**：
- Host侧：`export HCCL_HOST_SOCKET_PORT_RANGE="auto"`
- Device侧：`export HCCL_NPU_SOCKET_PORT_RANGE="auto"`

### TLS不一致
**现象**：两端建链均超时  
**检查**：
1. 查看LINK_ERROR_INFO表格中的TlsStatus
2. 执行`grep -r "TLS SWITCH" run/device-*/*.log`
3. 使用`hccn_tool -i $i -tls -g`

### 大集群故障定位
**难点**：rank间依赖关系复杂  
**方法**：
1. 使用建链根节点定位机制（自动探测）
2. 使用集群心跳机制（查看异常事件）
3. 开启HCCL_DIAGNOSE_ENABLE（A3/A2产品）

## 7. 详细诊断指南

如需详细的诊断步骤和案例分析，请加载对应阶段的子skill：

### 通信域初始化阶段详细诊断
```bash
/skill hccl_fault_stage1_init
```
包含：环境变量配置、rank table加载、集群协商、集群校验详细步骤

### 参数面建链阶段详细诊断
```bash
/skill hccl_fault_stage2_link
```
包含：建链失败定位、参数校验、网络连通性详细步骤

### 任务下发执行阶段详细诊断
```bash
/skill hccl_fault_stage3_exec
```
包含：集群心跳机制、task exception、SDMA/CQE错误详细步骤

## 8. 故障排查检查清单

### 基础检查
- [ ] 收集全量CANN日志（所有节点的debug和run目录）
- [ ] 确认故障码（EI****）和故障阶段
- [ ] 确认CANN版本是否一致
- [ ] 确认HCCL环境变量配置

### 通信域初始化阶段
- [ ] 检查rank table文件路径和权限
- [ ] 检查rank table字段配置完整性
- [ ] 检查device_ip配置是否与实际一致
- [ ] 检查Host侧端口是否被占用
- [ ] 检查TLS开关是否一致
- [ ] 检查IP Family是否一致（IPv4/IPv6）

### 参数面建链阶段
- [ ] 确认建链失败对端信息
- [ ] 检查对端日志和行为
- [ ] 检查网络连通性（hccn_tool ping）
- [ ] 检查TLS开关一致性
- [ ] 检查算子调用一致性（tag、cmdType等）
- [ ] 检查超节点配置（phySuperPodId）

### 任务下发执行阶段
- [ ] 检查心跳异常事件日志
- [ ] 定位集群故障根节点
- [ ] 检查异常类型（LOST/STUCK/Error CQE）
- [ ] 检查进程状态和core dump情况
- [ ] 检查网络状态和交换机配置
- [ ] 检查算子下发行为一致性

## 9. 日志文件说明

### CANN日志目录结构
```
log/
├── debug/plog/     # ERROR级别日志，故障诊断主要依据
├── run/plog/       # INFO级别日志，运行状态记录
└── run/device-*/   # Device侧日志，TLS、网络状态等
```

### 关键日志说明

#### 通信域初始化日志
```
Entry-HcclGetRootInfo:rootInfo[...], deviceLogicId[0]
Entry-HcclCommInitRootInfoInner:ranks[16], rank[0], ...
```
- ranks：通信域大小
- rank：当前rank编号
- identifier：通信域名

#### 算子下发日志（需开启HCCL_ENTRY_LOG_ENABLE）
```
Entry-HcclAllReduce: tag[...], sendBuf[...], recvBuf[...], 
count[...], dataType[...], op[...], localRank[...], streamId[...]
```

#### 通信域关键信息
```
[Communicator Key Info]identifier[...] rankSize[...] serverNum[...]
```

#### 本端关键信息
```
[LocalRank Key Info]userRank[...] hostIp[...] devicePhyId[...] 
server[...] deviceIp[...] superPodId[...]
```

## 10. cmdType和op枚举值速查

### cmdType枚举值（算子类型）
| 枚举值 | 算子类型 |
|--------|----------|
| 1 | BroadCast |
| 2 | AllReduce |
| 3 | Reduce |
| 4 | Send |
| 5 | Receive |
| 6 | AllGather |
| 7 | ReduceScatter |
| 8 | AlltoAllV |
| 9 | AlltoAllVC |
| 10 | AlltoAll |
| 11 | Gather |
| 12 | Scatter |
| 13 | BatchSendRecv |
| 16 | AllGatherV |
| 17 | ReduceScatterV |

### op枚举值（规约类型）
| 枚举值 | 规约类型 |
|--------|----------|
| 0 | SUM |
| 1 | PROD |
| 2 | MAX |
| 3 | MIN |
| 255 | 非Reduce算子 |

---

**使用建议**：
1. 首先使用故障码速查表和一键诊断命令快速定位问题类型和阶段
2. 根据阶段加载对应的详细诊断子skill进行深入分析
3. 参考检查清单进行系统化排查
4. 对于EI9999或无法解决的故障，请联系技术支持