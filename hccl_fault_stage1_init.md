# HCCL故障诊断 - 通信域初始化阶段详细指南

本文档提供通信域初始化阶段的详细故障诊断步骤和案例分析。

## 阶段概述

通信域初始化阶段包括以下关键步骤：
1. 环境变量初始化和资源初始化
2. rank table文件加载（或集群信息协商）
3. 集群信息校验

## 1. 环境变量配置异常 (EI0001)

### 定位思路
在业务日志中出现"EI0001"故障码，CANN日志ERROR级别会显示配置异常的环境变量名称、错误原因及合理配置范围。

### 常见场景

#### HCCL_RDMA_SL配置错误

**问题现象**
```text
[PID:3729526]Config_Error_Invalid_Environment_Variable(EI0001): 
Value 1000 for environment variable HCCL_RDMA_SL is invalid. 
Expected value : range[0, 7].
```

CANN日志：
```text
[ERROR]HCCL(3729526,python3.11):2025-10-23-17:30:40.098.973 [externalinput.cc:963] 
[3729526][Parse][rdmaServerLevel]HCCL_RDMA_SL[1000] is invalid. except: [0, 7]
```

**解决方法**
- HCCL_RDMA_SL有效范围：[0, 7]
- 根据日志建议调整配置参数

#### HCCL_SOCKET_IFNAME配置错误

**问题现象**
```text
[ERROR] HCCL(925892,alltoall_test):2025-10-28-16:34:59.634.432 [sal.cc:501] 
[925892][InitGroupStage][EnvConfig]set ifname to [abc] by HCCL_SOCKET_IFNAME, 
but not found in the environment, ifnames in the environment is as follows
[ERROR] HCCL(925892,alltoall_test):2025-10-28-16:34:59.634.437 [sal.cc:504] 
[925892][InitGroupStage][EnvConfig]get host ip fail by socket Ifname. 
name[lo] ip[127.10.0.1%lo]
```

**问题根因**
- 通过HCCL_SOCKET_IFNAME指定了Host网卡，但环境中不存在该网卡
- 报错日志列举了当前环境查询到的Host网卡

**解决方法**
- 修改HCCL_SOCKET_IFNAME，指定为环境上存在的Host网卡
- 容器场景需指定容器内可用的Host网卡

### 查询命令

查看当前进程环境变量配置：
```bash
grep -r "HCCL_ENV" run/plog/plog-*.log
```

## 2. rank table文件加载失败 (EI0004)

### 定位思路
基于rank table创建通信域时，若文件路径不存在、无权限或文件格式/配置错误，HCCL会加载失败。

### 常见场景

#### rank table文件路径不存在/权限不足

**问题现象**
```text
[ERROR] HCCL(1104629,test_one_side):2025-10-28-17:45:13.679.684 [param_check.cc:66] 
[1104629][InitGroupStage][RanktableConfig]errNo[0x0000000005010001] 
path /ranktable.json is not a valid real path
```

**解决方法**
- 检查rank table文件路径是否正确
- 检查文件权限是否可读

#### rank table字段配置错误（Atlas A3产品）

**问题现象**
```text
[ERROR] HCCL(1265,):2025-10-21 07:56:47.198.454 [topoinfo_ranktableConcise.cc:727]
[15326][InitGroupStage][RanktableCheck]errNo[0x0000000005010001] 
super_device_id[] is invalid
```

**问题根因**
- rank table的"version"字段为"1.2"
- 但"super_device_id"字段填写为空

**解决方法**
- 在rank table文件中补充"super_device_id"字段

#### rank table中device_ip不一致 (EI0014)

**问题现象**
```text
[ERROR] HCCP(166192,eExecutor):2025-01-21-16:59:39.962.565 [ra_host.c:480]
tid:167056,ra_rdev_init_check_ip(480) : [check][ip]fail, ret(129) 
the IP address(127.10.0.0) in the ranktable is inconsistent with the 
IP address(127.10.0.1) of the network adapter, please make sure they're consistent.
```

**问题根因**
- 当前device侧获取的device ip与rank table中配置的device ip不一致
- 例如：rank0的device实际ip为127.10.0.1，但rank table配置为127.10.0.0

**解决方法**
- 检查rank table配置与每个rank实际device ip是否一致

### 查询命令

排查rank table文件问题：
```bash
grep -rE "is not a valid real path|RanktableCheck" debug/plog/plog-*.log
```

## 3. 集群信息协商相关 (EI0015, EI0019)

### 业务流程

基于root节点信息创建通信域的场景：

1. **server节点**（通常是rank0）调用HcclGetRootInfo
   - 获取host网卡IP及端口信息生成rootInfo
   - Host网卡可通过HCCL_SOCKET_IFNAME指定
   - 端口可通过HCCL_IF_BASE_PORT指定（默认60000）
   - 绑定监听，启动后台线程等待所有agent连接

2. **rank节点**调用HcclCommInitRootInfo
   - 通过host网卡与server建立socket连接
   - 发送自己的rankInfo信息
   - 等待server返回完整集群信息
   - 超时时间：HCCL_CONNECT_TIMEOUT（默认120s）

3. **server节点**收集并发送集群信息
   - 收集全部rank的rankInfo信息
   - 生成完整集群信息
   - 发送给每个rank

### 常见场景

#### server节点端口绑定失败 (EI0019)

**问题现象**
```text
[PID: 2267203] 2025-11-21-11:38:29.575.404 
Communication_Error_Bind_IP_Port(EI0019): Failed to enable listening for the 
host network adapter socket.Reason: The IP address 192.168.1.100 and port 50001 
have already been bound.
```

CANN日志：
```text
[ERROR] HCCL(3626636,all_reduce_test):2025-11-21-13:18:47.639.860 [hccl_socket.cc:110] 
[3626636][InitChannelStage][RanktableDetect] socket type[2], 
listen on ip[192.168.1.100%enp53s0f2] and specific port[60000] fail. 
Please check the port status and whether the port is being used by other process.
```

**关键信息**
- socket type[2] 表示Host侧端口绑定失败（通信域协商阶段）
- socket type[0/1] 表示Device侧端口绑定失败（参数面建链阶段）

**解决方法**
- 通过HCCL_IF_BASE_PORT指定起始端口
- 多进程场景：配置HCCL_HOST_SOCKET_PORT_RANGE="auto"

#### 部分rank未连接到server节点 (EI0015)

**问题现象**

server节点：
```text
[ERROR] HCCL(1041081,all_reduce_test):2025-11-21-01:20:01.624.966 
[topoinfo_exchange_server.cc:314] [1041362][InitGroupStage][RanktableDetect]
topo exchange server get socket timeout! timeout[120 s]
[ERROR] HCCL(1041081,all_reduce_test):2025-11-21-01:20:01.625.145 
[topoinfo_exchange_server.cc:517] [1041362][InitGroupStage][DisplayConnectionedRank]
connected rankinfo[LINE 0]: [0000000000000000],[0000000000000002],[0000000000000004],[0000000000000006];
```

agent节点：
```text
[ERROR] HCCL(1041085,all_reduce_test):2025-11-21-01:20:01.630.122 
[topoinfo_exchange_base.cc:145] [1041085][InitGroupStage][RanktableDetect]
TopoDetect ERROR occur !!! fault_type[1], fault_info["Failed to connect agent[1,3,5,7,]"]
```

**定位思路**

1. **确认未连接的rank**
   - server节点：从已连接rank信息中计算缺失rank（如缺rank1,3,5,7）
   - agent节点：直接从"Failed to connect agent[]"报错中获取

2. **确认未连接rank是否有下发通信域创建**
   ```bash
   grep -r "Entry-HcclCommInitRootInfoInner" | grep "rank[9]"
   ```

3. **若下发但报错**：查看未连接rank的CANN日志排查报错原因

4. **若未下发**：从业务上排查该rank没有下发的原因

#### rank与server节点建立socket超时 (EI0015)

**问题现象**
```text
[ERROR] HCCL(7988,all_reduce_test):2025-03-19-04:16:13.978.979 
[topoinfo_exchange_agent.cc:190] [7988][InitGroupStage][RanktableDetect]
topo exchange agent get socket timeout! timeout[120]
[ERROR] HCCL(7988,all_reduce_test):2025-03-19-04:16:13.978.995 
[topoinfo_exchange_agent.cc:41] [7988][TopoInfoExchangeAgent][Setup]
TopoExchangeAgent: connect server[127.10.0.1 : 60000] failed
```

**定位思路**

1. **检查Host侧网络连通性**
   ```bash
   grep -r "get host ip success\|find nic.*success"
   ```
   - HCCL默认按字典序选择Host网卡，可能选择到不连通网卡
   - 通过HCCL_SOCKET_IFNAME指定正确网卡

2. **检查每个agent下发时间间隔**
   ```bash
   grep -r "Entry-HcclGetRootInfo\|Entry-HcclCommInitRootInfoInner" run/plog
   ```

**示例分析**：
```text
[INFO] HCCL(3079955,all_reduce_test):2025-11-20-11:59:56.716.583 
Entry-HcclCommInitRootInfoInner:ranks[4], rank[3], ... (rank3下发时间)
[INFO] HCCL(3079952,all_reduce_test):2025-11-20-11:56:36.704.523 
Entry-HcclGetRootInfo:rootInfo[...], deviceLogicId[0] (rank0下发时间)
```
- rank3比rank0慢了200秒，超时时间120秒 → 等待超时
- 解决：调大HCCL_CONNECT_TIMEOUT或优化业务加载时间

### 查询命令

排查集群协商问题：
```bash
# server节点端口绑定失败
grep -rE "socket type\[2\].*Please check the port" debug/plog/plog-*.log

# server未收到全部rank连接
grep -r "topo exchange server get socket timeout" debug/plog/plog-*.log

# rank与server建立socket超时
grep -r "topo exchange agent get socket timeout" debug/plog/plog-*.log

# 查看当前Host网卡
grep -r "get host ip success\|find nic.*success" run/plog/plog-*.log

# 查看通信域创建下发时间
grep -r "Entry-HcclGetRootInfo\|Entry-HcclCommInitRootInfoInner" run/plog
```

## 4. 集群信息校验失败 (EI0001, EI0014, EI0016)

### 定位思路
HCCL会对rank table信息进行校验，若校验失败直接报错退出。

### 常见场景

#### IP Family校验不一致 (EI0001)

**问题现象**
```text
[ERROR] HCCL(144905,python):2025-04-20-00:26:54.435.048 [config.cc:413] 
[145735][InitGroupStage][RanktableCheck]rank[0] device ip family[2] 
is not same as others[10].
```

**问题根因**
- 两个rank的IP Family不同（IPv4 vs IPv6）
- family枚举值：2=IPv4, 10=IPv6

**解决方法**
```bash
# 查询IPv4配置
hccn_tool -i {deviceId} -ip -g

# 查询IPv6配置
hccn_tool -i {deviceId} -ip -inet6 -g
```
- 确保所有rank的IP Family一致
- HCCL默认使用IPv4，若未配置IPv4则使用IPv6
- 通过HCCL_SOCKET_FAMILY指定IP协议

#### TLS信息配置不一致 (EI0016)

**问题现象**
```text
[ERROR] HCCL(94774,all_reduce_test):2025-10-27-11:51:32.570.490 
[topoinfo_exchange_agent.cc:831] [94774][InitGroupStage][RanktableCheck] 
Value Disable for config "tls" is invalid. Expected Value:"All ranks are consistent. 
Current status : rankList for enabled tls:[10.78.106.107/0]; 
rankList for disabled tls:[10.78.106.107/0]; ...
```

**问题根因**
- 通信域内TLS开关状态不一致
- 仅支持Ascend HDK 25.2.0以上版本及root协商场景

**解决方法**
```bash
# 查询TLS状态
hccn_tool -i {device_id} -tls -g

# 查询所有Device TLS状态
for i in `seq 0 7`; do hccn_tool -i $i -tls -g; done
```

输出示例：
```text
dev_id:0, tls switch[0](0:disable, 1:enable), tls alarm time threshold[60]days
dev_id:0, [pub cert] info:
         issuer[/C=CN/ST=GD/O=HUAWEI/OU=2012/CN=2_1thCA]
         start_time[Wed Feb 19 03:19:21 2020 GMT]
         end_time[Sat Feb 16 03:19:21 2030 GMT]
```

**统一TLS配置**
```bash
# 开启TLS
hccn_tool -i {device_id} -tls -s enable 1

# 关闭TLS
hccn_tool -i {device_id} -tls -s enable 0

# 替换证书套件
hccn_tool -i 0 -tls -s path /root pri pri.pem pub pub.pem ca1 ca1.pem ca2 ca2.pem crl xxx.crl
```

#### superDeviceId重复 (EI0014)

**问题现象**
```text
[ERROR] HCCL(169030,alltoall_test):2025-10-23-16:28:59.392.635 
[topoinfo_exchange_agent.cc:695] [169030][InitGroupStage][RanktableCheck]
devices have same superDeviceId[0x3000000] in superPod[super_pod_id_0]. 
Current device info: serverId[127.10.0.1], rankId[0], group[hccl_world_group]. 
Another device info: rankId[1].
```

**问题根因**
- superDeviceId是Atlas A3产品在超节点系统中的物理ID
- 一个超节点内有相同的superDeviceId → 校验失败

**查询superDeviceId**
```bash
npu-smi info -t spod-info -i id -c chip_id
```

**可能原因**
1. 硬件配置异常
2. 通过HCCL_LOGIC_SUPERPOD_ID将不同物理超节点配置为同一逻辑超节点

**解决方法**
- 修改硬件配置
- 正确配置HCCL_LOGIC_SUPERPOD_ID环境变量

### 查询命令

排查集群校验问题：
```bash
# IP Family不一致
grep -r "device ip family\[2\] is not same as others\[10\]" debug/plog/plog-*.log

# TLS不一致
grep -rE "tls.*invalid|All ranks are consistent" debug/plog/plog-*.log

# superDeviceId重复
grep -r "superDeviceId.*already exist" debug/plog/plog-*.log

# 查询TLS状态
grep -r "TLS SWITCH" run/device-*/*.log
hccn_tool -i {device_id} -tls -g

# 查询IP配置
hccn_tool -i {device_id} -ip -g
```

## 5. 典型案例分析

### 案例：三机24卡通信域创建失败

**问题现象**

三机共24卡通信域创建协商超时：

- **节点0（root节点）**：
```text
[ERROR] topo exchange server get socket timeout! timeout[120 s]
connected rankinfo[LINE 0]: [0,2,4,6,8,10,12,14];  # 已连接8个rank
```
- 缺失rank：16-23（节点2的rank）

- **节点1**：
```text
[ERROR] TopoDetect ERROR occur !!! fault_info["Failed to connect agent[16-23]"]
```

- **节点2**：
```text
[ERROR] topo exchange agent get socket timeout! 
TopoExchangeAgent: connect server[127.10.0.1 : 60000] failed
```

**问题根因**
- 节点2与root节点Host侧网络配置错误，无法连接

**解决方法**
- 检查节点2的网络配置
- 配置HCCL_SOCKET_IFNAME指定正确网卡
- 检查Host侧端口连通性

## 6. 检查清单

### 环境变量检查
- [ ] HCCL_RDMA_SL范围是否正确[0,7]
- [ ] HCCL_SOCKET_IFNAME是否指定正确网卡
- [ ] HCCL_CONNECT_TIMEOUT是否合理（默认120s）
- [ ] HCCL_IF_BASE_PORT端口是否被占用

### rank table检查
- [ ] 文件路径是否正确
- [ ] 文件权限是否可读
- [ ] 字段配置是否完整（version, super_device_id等）
- [ ] device_ip是否与实际一致

### 集群协商检查
- [ ] Host网卡是否连通
- [ ] Host端口是否连通
- [ ] 每个rank下发时间是否在超时时间内
- [ ] server节点是否收到所有rank连接

### 集群校验检查
- [ ] 所有rank IP Family是否一致
- [ ] 所有rank TLS状态是否一致
- [ ] superDeviceId是否重复
- [ ] 超节点配置是否正确

---

**相关命令**：
- `/skill hccl_fault_diagnosis_main` - 返回主诊断指南
- `/skill hccl_fault_stage2_link` - 参数面建链阶段诊断
- `/skill hccl_fault_stage3_exec` - 任务下发执行阶段诊断