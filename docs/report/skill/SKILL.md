---
name: md-to-puml-sequence
description: Use when converting markdown analysis documents into simplified PlantUML sequence diagrams, for presenting overview flow while omitting implementation details
---

# Markdown 分析文档 → PlantUML 简化时序图

## Overview

将 markdown 分析文档转化为 PlantUML 简化时序图。核心原则：**介绍大体流程，省略实现细节**。比原版时序图少 50%+ 的 participant 和交互步骤。

## When to Use

- 已有 markdown 分析文档，需要生成配套的简化时序图
- 原始时序图过于详细，需要一个"概览版"
- 需要快速向他人介绍某个数据流/调用链的全局结构

## Core Workflow

### Step 1: 读文档，理解全局

读 markdown 文档，回答三个问题：
1. 有几个参与者？（类、模块、函数）
2. 有几段主要流程？（路径 A、路径 B、汇合等）
3. 关键数据是什么？从哪里来，到哪里去？

### Step 2: 读源码，验证关键逻辑

读文档中引用的关键源码行（不可跳过）：
- 函数入口/出口
- 核心循环或分支
- 数据写入点

目标：确保图上的箭头标签和源码一致，不是凭文档猜测。

### Step 3: 合并 Participant

原则：能合就合，目标 ≤ 7 个 participant。

```
合并前（00 详细版）:  合并后（简化版）:
hcclComm              hcclComm + CollComm  → Framework
CollComm              
hccl::RankGraph       基类 + V2 包装        → V2
RankGraphV2           
```

用 `\n` 在 participant 标签中标注合并来源：
```
participant "hcclComm\n+CollComm" as Framework
```

### Step 4: 用 == 分隔段落

每段一个清晰主题，用 ASCII 英文（Jetty PlantUML 对中文分隔线兼容差）：

```
== Write Path - BuildRankDescVec ==
== Read Path - V2 Query ==
== V1 Fallback ==
```

### Step 5: 箭头只写"做什么"，不写"怎么做"

```
# 简化版 — 概括意图
DataSrc -> DataSrc: populate all 8 fields from rankTable and peers

# 详细版 — 逐字段列出
DataSrc -> DataSrc: d.localId = GetLocalId(i)
DataSrc -> DataSrc: d.serverIdx = rankInfo.serverIdx
DataSrc -> DataSrc: d.elecGroupId = lv.elecGroupId
...
```

### Step 6: Note 仅保留关键洞察

- 解释"为什么"（如：零拷贝、const_cast 桥接）
- 标注异常路径（如：V1 回退）
- 去掉"是什么"的描述性 note

## 本地 PlantUML Server 部署

```bash
# Docker 容器部署，Jetty 版本
sudo docker run -d --name plantuml-server -p 8888:8080 plantuml/plantuml-server:jetty

# 验证运行状态
sudo docker ps --filter "name=plantuml-server"
```

| 项目 | 值 |
|------|-----|
| 容器名 | `plantuml-server` |
| 镜像 | `plantuml/plantuml-server:jetty` |
| 本地端口 | `8888` |
| 版本 | Jetty 1.2026.2 |

## Jetty PlantUML 兼容规则

Jetty 版本对以下写法敏感：

| 问题 | 错误写法 | 正确写法 |
|------|---------|---------|
| Self-arrow 中的 `()` | `DataSrc -> DataSrc: vec.size()` | `DataSrc -> DataSrc: vec size` |
| `==` 分隔线中的中文 | `== 写入路径 ==` | `== Write Path ==` |
| 单行 note 中的 `\n` | `note over X: line1\nline2` | `note over X: line1 line2` |
| Self-arrow 中的 `<>` | `map<u32, RankId>` | 用文字描述 |

**注意**：非 self-arrow 的消息中 `()` 和 `[]` 通常没问题，仅 self-arrow 敏感。

## Validation Workflow

每次写完 PUML 后必须验证：

```bash
# 1. 用 /txt/ 端点验证
curl -s -X POST --data-binary @<file.puml> http://localhost:8888/txt/ 2>&1 | grep -c "Syntax Error"
# 期望输出: 0

# 2. 如有错误，定位行号
curl -s -X POST --data-binary @<file.puml> http://localhost:8888/txt/ 2>&1 | grep "From string"

# 3. 确认渲染完整
curl -s -X POST --data-binary @<file.puml> http://localhost:8888/txt/ 2>&1 | wc -l
```

## File Naming

放在与源文档相同的 `flow/` 目录下，序号递增：

```
flow/
  00-<主题>-详细时序图.puml    # 原详细版
  01-<主题>-简化时序图.puml     # 简化版 (本 skill 产出)
  02-<主题>-简化时序图.puml
  ...
```

## Common Mistakes

| 错误 | 后果 | 纠正 |
|------|------|------|
| 跳过 Step 2（不读源码） | 图上箭头标签与代码不一致 | 必须读关键行验证 |
| Participant 过多（>7） | 图横向撑爆，难以阅读 | 合并同类 participant |
| 箭头消息写得太细 | 与详细版无差异，失去"简化"意义 | 概括意图，一个箭头承载多步 |
| `==` 分隔线用中文 | Jetty PlantUML Syntax Error | 用英文分隔线 |
| Self-arrow 消息含 `()` | Syntax Error on that line | 去掉括号或用文字替代 |
| 写完不验证 | 语法错误未被发现 | curl /txt/ 端点验证 |
