# Code Review Report

**仓库**: hcomm  
**Commit**: 985540720375b1bc785c2387754b2027b8ca606a  
**作者**: hblnb <huangbolin3@huawei.com>  
**日期**: 2026-05-12  
**主题**: 解决重复创建channel时，只能创建一个socket导致后续channel创建失败的问题  

---

## 一、改动概述

### 1.1 改动目的
解决重复创建 channel 时，socket 复用机制存在缺陷，导致只能创建一个 socket，后续 channel 创建失败的问题。

### 1.2 核心方案
- 将 `SocketMgr` 从实例模式改为单例模式
- 引入 `socketInUseMap_` 跟踪 socket 使用状态
- 新增 `PutSocket()` 方法实现 socket 释放
- 各 Channel 类在适当时机调用 `PutSocket()` 释放 socket 引用

### 1.3 改动统计
| 指标 | 数量 |
|------|------|
| 修改文件数 | 19 |
| 新增行数 | 174 |
| 删除行数 | 38 |
| 净增加行数 | 136 |

---

## 二、改动详情

### 2.1 架构变更

#### 2.1.1 SocketMgr 单例化
**改动前**：各 Channel 类持有独立的 `std::unique_ptr<SocketMgr>` 实例

**改动后**：全局单例 `SocketMgr::GetInstance()`

**影响范围**：
- `AicpuTsP2pChannel`
- `AicpuTsUboeChannel`
- `AicpuTsUrmaChannel`
- `HostCpuRoceChannel`
- `HostCpuUrmaChannel`
- `EndpointPair`
- `SocketProcess`

#### 2.1.2 新增接口

| 接口 | 文件 | 功能 |
|------|------|------|
| `SocketMgr::PutSocket()` | socket_mgr.h/cc | 释放 socket 使用权 |
| `SocketMgr::GetInstance()` | socket_mgr.h/cc | 获取单例实例 |
| `SocketProcess::PutSocket()` | socket_process.h/cc | 封装调用 SocketMgr::PutSocket |
| `SocketRelease()` | hccl_comm_socket_c_adpt.h/cc | C API 接口释放 socket |

#### 2.1.3 新增数据结构

```cpp
std::unordered_map<Hccl::SocketConfig, bool> socketInUseMap_;  // 跟踪 socket 使用状态
std::condition_variable socketAvailableCv_;                      // 条件变量同步等待
```

### 2.2 生命周期管理变更

#### 2.2.1 Channel 类析构函数新增
所有 Channel 子类新增析构函数释放 socket：
```cpp
~AicpuTsP2pChannel()
{
    if (socket_ != nullptr) {
        SocketMgr::GetInstance().PutSocket(socket_);
        socket_ = nullptr;
    }
}
```

#### 2.2.2 GetStatus() 中提前释放
Channel 状态变为 `READY` 时立即释放 socket：
```cpp
ChannelStatus GetStatus()
{
    ChannelStatus out = ...;
    if (out == ChannelStatus::READY && socket_ != nullptr) {
        SocketMgr::GetInstance().PutSocket(socket_);
        socket_ = nullptr;
    }
    return out;
}
```

#### 2.2.3 ClusterMonitor 中新增释放
`cluster_monitor.cc:387` 新增：
```cpp
SocketRelease(needConnectRank.socketHandler);
```

---

## 三、问题发现

### 3.1 严重问题

#### 问题1：GetSocket 中存在数据竞争

**位置**: `socket_mgr.cc:166-177`

**问题描述**:  
`socketInUseMap_[socketConfig]` 的访问未持有锁，存在数据竞争。

**问题代码**:
```cpp
lock.unlock();
while(socketInUseMap_[socketConfig] == true) {  // 无锁访问！
    if (socketAvailableCv_.wait_until(lock, timeoutPoint) == std::cv_status::timeout) {
        HCCL_ERROR("[SocketMgr][%s] Get Socket Time Out", __func__);
        return HCCL_E_TIMEOUT;
    }
}
lock.lock();
```

**风险**: 多线程并发调用 `GetSocket` 时可能导致：
- 读到脏数据
- 条件变量等待逻辑异常
- 潜在的死锁或活锁

**建议修复**:
```cpp
while(true) {
    // wait_until 会自动释放锁，唤醒后重新获取锁
    if (socketInUseMap_[socketConfig] == false) break;
    if (socketAvailableCv_.wait_until(lock, timeoutPoint) == std::cv_status::timeout) {
        HCCL_ERROR("[SocketMgr][%s] Get Socket Time Out", __func__);
        return HCCL_E_TIMEOUT;
    }
}
```

---

### 3.2 中等问题

#### 问题2：全局变量 g_linkTimeout 非线程安全初始化

**位置**: `socket_mgr.cc:19-23`

**问题代码**:
```cpp
s32 g_linkTimeout = 0;
inline s32 EnvLinkTimeoutGet()
{
    g_linkTimeout = g_linkTimeout != 0 ? g_linkTimeout : 
                     Hccl::EnvConfig::GetInstance().GetSocketConfig().GetLinkTimeOut();
    return g_linkTimeout;
}
```

**风险**: 多线程首次并发调用时可能多次初始化

**建议修复**:
```cpp
static std::once_flag g_linkTimeoutFlag;
static s32 g_linkTimeout = 0;

inline s32 EnvLinkTimeoutGet()
{
    std::call_once(g_linkTimeoutFlag, []() {
        g_linkTimeout = Hccl::EnvConfig::GetInstance().GetSocketConfig().GetLinkTimeOut();
    });
    return g_linkTimeout;
}
```

---

#### 问题3：GetStatus 与析构函数中的双重释放

**位置**: 各 Channel 类的 `GetStatus()` 和析构函数

**问题描述**:  
- `GetStatus()` 在状态为 READY 时释放 socket 并置空
- 析构函数中也会检查并释放 socket

**当前代码**:
```cpp
// GetStatus()
if (out == ChannelStatus::READY && socket_ != nullptr) {
    SocketMgr::GetInstance().PutSocket(socket_);
    socket_ = nullptr;
}

// 析构函数
~AicpuTsP2pChannel()
{
    if (socket_ != nullptr) {
        SocketMgr::GetInstance().PutSocket(socket_);
        socket_ = nullptr;
    }
}
```

**分析**:  
- 由于 `GetStatus()` 已将 `socket_` 置空，析构时不会重复释放
- 逻辑上是安全的，但存在代码冗余
- 如果对象在 READY 状态下被销毁，析构函数中的检查是必要的（防御性编程）

**建议**:  
保持现状，这是合理的防御性编程。可选优化：在基类 Channel 中统一管理 socket 生命周期。

---

#### 问题4：PutSocket 查找效率问题

**位置**: `socket_mgr.cc:199-213`

**问题代码**:
```cpp
void SocketMgr::PutSocket(Hccl::Socket*& socket)
{
    for (auto it = socketMap_.begin(); it != socketMap_.end(); ++it) {
        if (it->second.get() == socket) {
            // ...
        }
    }
}
```

**问题**:  
- 通过遍历 `socketMap_` 查找 socket 指针，时间复杂度 O(n)
- 高频调用时可能成为性能瓶颈

**建议修复**:
```cpp
// 新增反向映射
std::unordered_map<Hccl::Socket*, Hccl::SocketConfig> socketToConfigMap_;

void SocketMgr::PutSocket(Hccl::Socket*& socket)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = socketToConfigMap_.find(socket);
    if (it != socketToConfigMap_.end()) {
        socketInUseMap_[it->second] = false;
        socketAvailableCv_.notify_all();
    }
}
```

---

### 3.3 轻微问题

#### 问题5：缺少错误日志

**位置**: `socket_mgr.cc:199-213`

**问题描述**:  
`PutSocket` 找不到 socket 时只打印 ERROR 日志，调用方无感知。

**建议**:  
考虑返回 `HcclResult` 让调用方感知错误。

---

#### 问题6：代码风格不一致

**位置**: 多处

**问题描述**:
- `aicpu_ts_urma_channel.cc:277` 多余空行
- 部分文件 `if (cond)` 后有空格，部分没有

**建议**:  
统一代码风格，移除多余空行。

---

## 四、改进建议

### 4.1 必须修复
| 优先级 | 问题 | 影响 | 建议 |
|--------|------|------|------|
| **P0** | GetSocket 数据竞争 | 多线程并发时可能导致死锁或异常 | 修复条件变量等待逻辑 |

### 4.2 建议修复
| 优先级 | 问题 | 影响 | 建议 |
|--------|------|------|------|
| P1 | g_linkTimeout 非线程安全 | 可能多次初始化 | 使用 std::call_once |
| P2 | PutSocket 线性查找 | 高频调用性能影响 | 增加反向映射 |

### 4.3 可选优化
| 优先级 | 问题 | 建议 |
|--------|------|------|
| P3 | socket 生命周期管理分散 | 考虑在 Channel 基类统一管理 |
| P3 | 代码风格不一致 | 统一格式化 |

---

## 五、测试建议

### 5.1 单元测试
- [ ] SocketMgr 单例模式线程安全性测试
- [ ] GetSocket/PutSocket 并发压力测试
- [ ] socketInUseMap_ 状态一致性测试
- [ ] 条件变量超时机制测试

### 5.2 集成测试
- [ ] 重复创建 channel 场景测试（原问题复现场景）
- [ ] 多线程并发创建 channel 测试
- [ ] Channel 生命周期管理测试
- [ ] Socket 复用场景测试

### 5.3 压力测试
- [ ] 高并发场景下 socket 创建/释放性能测试
- [ ] 长时间运行稳定性测试

---

## 六、总结

本次改动通过将 `SocketMgr` 改为单例模式并引入引用计数机制，解决了 socket 复用问题，整体思路正确。但存在一个严重的数据竞争问题需要立即修复。建议：

1. **立即修复** GetSocket 中的数据竞争问题
2. **建议修复** g_linkTimeout 的线程安全初始化
3. **可选优化** PutSocket 查找效率

---

**Review 状态**: ⚠️ **需要修改**  
**审查人**: CANNBot  
**审查日期**: 2026-05-14