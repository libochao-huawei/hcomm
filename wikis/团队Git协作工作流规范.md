# HCOMM 团队 Git 协作工作流规范

## 文档概述

本文档定义 HCOMM 项目的 Git 协作工作流规范，确保团队成员高效协作，代码质量可控。

**版本**: v1.1
**生效日期**: 2026-02-11
**适用范围**: HCOMM 项目全体开发人员
**仓库地址**: https://gitcode.com/qq_30335613/8k_oxc_hcomm

---

## 1. 工作流概述

### 1.1 核心原则

- **master 受保护**: 只有管理员可以合入代码，其他成员通过 PR 提交
- **两层 PR 审核**: personal → feature → master，确保代码质量和功能完整性
- **个人工作分支**: 每个开发者维护自己的 `personal/<username>` 分支，用于日常开发
- **功能保护分支**: `feature/<name>` 作为代码收集和审核缓冲区
- **代码评审**: 所有合入必须经过 Code Review

### 1.2 两层 PR 工作流（推荐）

```
master (受保护，管理员合入)
  ↑
  └─ feature/oxc-ranktable (功能保护分支，向 master 提 PR)
        ↑
        └─ personal/zhangzhanbang (个人工作分支，向 feature 提 PR)
```

**工作流程**：
1. **日常开发**: 在 `personal/<username>` 分支上进行
2. **第一层 PR**: personal → feature（代码积累到一定程度提 PR）
3. **第二层 PR**: feature → master（功能完整后提 PR）

**优势**：
- ✅ 个人分支可以自由提交，不影响他人
- ✅ feature 分支作为"代码审核缓冲区"
- ✅ 两层审核，代码质量更高
- ✅ 功能完整后再合入 master

### 1.3 分支结构

```
master                              # 主线分支（受保护）
  │
  ├── personal/zhangzhanbang        # 张占榜的个人工作分支
  │     └── (日常开发在此进行)
  │
  ├── feature/oxc-ranktable         # OXC rank table 功能分支
  │     └── (从 personal 合并代码)
  │
  └── feature/another-feature       # 其他功能分支
        └── (从 personal 合并代码)
```

### 1.4 分支类型说明

| 分支类型 | 命名格式 | 说明 | 生命周期 | 权限 |
|---------|---------|------|---------|------|
| **主分支** | `master` | 生产就绪代码，始终保持稳定 | 永久 | 管理员维护，其他人只读 |
| **个人分支** | `personal/<username>` | 开发者的个人工作基线，日常开发在此 | 长期维护 | 所有者可读写，其他人只读 |
| **功能分支** | `feature/<name>` | 功能保护分支，从 personal 合并代码 | 中期维护，功能完成后删除 | 创建者可读写 |
| **修复分支** | `fix/<name>` | Bug 修复（紧急情况直接从 master 创建） | 临时，合入后删除 | 创建者可读写 |
| **重构分支** | `refactor/<name>` | 代码重构 | 临时，合入后删除 | 创建者可读写 |
| **文档分支** | `docs/<name>` | 文档更新 | 临时，合入后删除 | 创建者可读写 |

---

## 2. 开发环境初始化

### 2.1 首次克隆仓库

```bash
# 克隆仓库
git clone <repository-url> hcomm
cd hcomm

# 配置用户信息（如果尚未配置）
git config user.name "你的姓名"
git config user.email "你的邮箱"

# 查看远程仓库
git remote -v
```

### 2.2 创建个人分支（第一次必须）

```bash
# 确保本地 master 是最新的
git checkout master
git pull origin master

# 创建个人分支（以用户名 aircat 为例）
git checkout -b personal/aircat

# 推送到远程仓库
git push -u origin personal/aircat

# 设置个人分支为默认分支（可选）
# 在 Git 平台的仓库设置中，将个人分支设为默认
```

### 2.3 配置 Git Alias（推荐）

在 `~/.gitconfig` 或项目的 `.git/config` 中添加以下别名：

```bash
git config --global alias.new-feature '!f() { git checkout personal/$(git config user.name | cut -d" " -f1 | tr "[:upper:]" "[:lower:]") && git pull && git checkout -b feature/$1; }; f'
git config --global alias.finish-feature '!f() { git checkout personal/$(git config user.name | cut -d" " -f1 | tr "[:upper:]" "[:lower:]") && git pull && git merge feature/$1 && git push && git branch -d feature/$1; }; f'
git config --global alias.sync-master '!git checkout master && git pull && git checkout personal/$(git config user.name | cut -d" " -f1 | tr "[:upper:]" "[:lower:]") && git merge master && git push'
```

> **注意**: 上面的 alias 假设用户名格式为 "Zhang San"，会自动提取 "zhang" 作为个人分支名。如果你的用户名不是这个格式，需要手动调整。

简化版本（直接指定用户名）：

```bash
# 将 aircat 替换为你的用户名
git config --global alias.co-personal 'checkout personal/aircat'
git config --global alias.sync-personal '!git checkout personal/aircat && git pull'
git config --global alias.sync-master '!git checkout master && git pull && git checkout personal/aircat && git merge master && git push'
```

---

## 3. 标准开发流程（两层 PR 工作流）

### 3.1 创建分支结构（首次）

```bash
# 1. 确保本地 master 是最新的
git checkout master
git pull origin master

# 2. 创建个人分支（长期维护）
git checkout -b personal/zhangzhanbang
git push -u origin personal/zhangzhanbang

# 3. 创建功能保护分支（从 master 创建）
git checkout master
git checkout -b feature/oxc-ranktable
git push -u origin feature/oxc-ranktable

# 切回个人分支，开始日常开发
git checkout personal/zhangzhanbang
```

### 3.2 在个人分支上日常开发

```bash
# 1. 确保个人分支最新
git checkout personal/zhangzhanbang
git pull origin personal/zhangzhanbang

# 2. 创建临时功能分支（可选，用于隔离开发）
git checkout -b feature/dev-oxc-parser

# 3. 在功能分支上进行开发
# 编辑代码、编译、测试
vim src/framework/common/src/topo/topoinfo_ranktableOxc.cc

# 4. 提交代码
git add src/framework/common/src/topo/topoinfo_ranktableOxc.cc
git commit -m "feat(topo): 添加 OXC 拓扑结构解析支持

- 实现 OXC 拓扑识别逻辑
- 添加拓扑层级解析
- 更新单元测试用例

Refs: #HCCL-1234"

# 5. 合并回个人分支
git checkout personal/zhangzhanbang
git merge --no-ff feature/dev-oxc-parser  # --no-ff 保留分支历史
git push origin personal/zhangzhanbang

# 6. 删除临时功能分支（可选）
git branch -d feature/dev-oxc-parser
```

**注意**: 也可以直接在 `personal` 分支上开发，不创建临时功能分支。

### 3.3 第一层 PR：personal → feature

当代码积累到一定程度，向 feature 分支提 PR。

#### 步骤 1: 确保个人分支最新

```bash
git checkout personal/zhangzhanbang
git pull origin personal/zhangzhanbang
```

#### 步骤 2: 创建 Pull Request（personal → feature）

**方式A: 使用 GitCode API（推荐）**

```bash
curl -X POST "https://gitcode.com/api/v5/repos/qq_30335613/8k_oxc_hcomm/pulls" \
  -H "Authorization: Bearer <你的Access Token>" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "feat: 添加 OXC rank table 解析支持",
    "head": "personal/zhangzhanbang",
    "base": "feature/oxc-ranktable",
    "body": "PR 描述内容"
  }'
```

**方式B: 使用 GitCode Web 界面**

访问链接创建 PR：
```
https://gitcode.com/qq_30335613/8k_oxc_hcomm/merge_requests/new?source_branch=personal/zhangzhanbang&target_branch=feature/oxc-ranktable
```

- **源分支**: `personal/zhangzhanbang`
- **目标分支**: `feature/oxc-ranktable`
- **标题**: 使用约定式提交格式

#### 步骤 3: PR 描述模板（personal → feature）

```bash
# 1. 切换到个人分支，确保最新
git checkout personal/aircat
git pull origin personal/aircat

# 2. 从个人分支创建功能分支
git checkout -b feature/oxc-topology-support

# 3. 在功能分支上进行开发
# 编辑代码、编译、测试
vim src/framework/common/src/topo/topoinfo_ranktableOxc.cc

# 4. 提交代码
git add src/framework/common/src/topo/topoinfo_ranktableOxc.cc
git commit -m "feat: 添加 OXC 拓扑结构解析支持

- 实现 OXC 拓扑识别逻辑
- 添加拓扑层级解析
- 更新单元测试用例

Refs: #HCCL-1234"

# 5. 开发过程中，定期同步个人分支的最新代码
git checkout personal/aircat
git pull origin personal/aircat
git checkout feature/oxc-topology-support
git merge personal/aircat

# 6. 功能开发完成后，合并回个人分支
git checkout personal/aircat
git merge --no-ff feature/oxc-topology-support  # --no-ff 保留分支历史
git push origin personal/aircat

# 7. 删除本地功能分支（可选）
git branch -d feature/oxc-topology-support

# 8. 删除远程功能分支（如果已推送）
git push origin --delete feature/oxc-topology-support
```

### 3.2 向 master 提交 PR

```markdown
## 变更概述
添加 OXC rank table 解析支持，实现 A5 场景下的 OXC 组网配置功能。

## 变更类型
- [x] 新功能 (feature)
- [ ] Bug 修复 (bugfix)
- [ ] 代码重构 (refactor)
- [ ] 文档更新 (docs)

## 主要改动

### 新增文件
- `src/framework/common/src/topo/topoinfo_ranktableOxc.cc` - OXC 拓扑解析实现
- `src/framework/common/src/topo/topoinfo_ranktableOxc.h` - OXC 拓扑解析头文件
- `test/ut/framework/common/topo/ut_topoinfo_ranktableOxc.cc` - 单元测试

### 修改文件
- `src/framework/common/src/topo/topoinfo_ranktableParser_pub.h` - 新增 OXC 解析接口声明
- `src/framework/inc/topoinfo_struct.h` - 扩展拓扑结构定义
- `src/legacy/framework/topo/new_topo_builder/common/topo_common_types.h` - 新增 OXC 相关类型定义

### 功能说明
- 支持 OXC 拓扑类型识别
- 实现 OXC rank table 格式解析
- 添加拓扑层级映射关系处理
- 单元测试覆盖核心场景

## 测试情况
- [x] 已通过本地编译验证
- [ ] 单元测试通过（待 CI 验证）
- [x] 代码自检完成
- [x] 符合编码规范

## 编译验证
```bash
cd /data/ccl_workspace/toolsh_dir
bash hcomm_compile.sh
# 编译状态: ✅ 通过
```

## 相关 Issue
- Feature: #HCCL-XXXX（请填写实际 Issue 编号）

## 检查清单
- [x] 代码符合《华为 C++ 可信编码 CleanCode 规范》
- [x] 已添加必要的单元测试
- [x] 函数职责单一，命名清晰
- [x] 错误处理完善
- [x] 无编译警告和错误
- [x] 已执行编译验证脚本
- [x] 未提交本地配置文件（.gitignore、CLAUDE.md、CMakeLists.txt 等）

## 备注
- 这是第一层 PR，将代码合并到 feature/oxc-ranktable 分支
- 等待功能完整后，再提 feature → master 的 PR

**变更文件统计**: 10 个文件，新增 777 行代码
```

#### 合并 PR

```bash
# PR 审核通过后，合并到 feature 分支
git checkout feature/oxc-ranktable
git pull origin feature/oxc-ranktable

# 或者通过 GitCode Web 界面点击 "Merge" 按钮
```

### 3.4 第二层 PR：feature → master

当 feature 分支的功能完整开发并通过测试后，向 master 提 PR。

#### 步骤 1: 确保 feature 分支最新

```bash
# 同步 master 的最新代码到 feature
git checkout master
git pull origin master
git checkout feature/oxc-ranktable
git merge master
git push origin feature/oxc-ranktable
```

#### 步骤 2: 创建 Pull Request（feature → master）

**使用 GitCode API**:

```bash
curl -X POST "https://gitcode.com/api/v5/repos/qq_30335613/8k_oxc_hcomm/pulls" \
  -H "Authorization: Bearer <你的Access Token>" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "【里程碑】OXC rank table 解析功能完整实现",
    "head": "feature/oxc-ranktable",
    "base": "master",
    "body": "PR 描述内容"
  }'
```

**使用 GitCode Web 界面**:
- **源分支**: `feature/oxc-ranktable`
- **目标分支**: `master`

#### 步骤 3: PR 描述模板（feature → master）

```markdown
## 变更概述
【里程碑】OXC rank table 解析功能已完整实现并通过测试，申请合并到主线。

## 变更类型
- [x] 新功能 (feature)
- [ ] Bug 修复 (bugfix)
- [ ] 代码重构 (refactor)
- [ ] 文档更新 (docs)

## 功能完整性确认

### 核心功能
- [x] OXC 拓扑类型识别
- [x] OXC rank table 格式解析
- [x] 拓扑层级映射关系处理
- [x] 错误处理和边界条件处理
- [x] 与现有 rank table 解析逻辑兼容

### 测试覆盖
- [x] 单元测试通过
- [x] 集成测试通过（如有）
- [x] 代码覆盖率达标
- [x] 性能测试通过（如有）

## 合并历史

本 PR 包含以下提交（从 personal → feature → master）：
- `feat(topo): 添加 OXC rank table 解析支持` (5c3c68a)
- <!-- 如有其他 commit，在此列出 -->

## 影响范围
- **模块**: 拓扑解析模块 (`src/framework/common/src/topo/`)
- **影响**: 向后兼容，不影响现有功能
- **配置**: 无需额外配置
- **文档**: 已更新相关注释

## 相关 Issue
- Closes #HCCL-XXXX
- Related to #HCCL-YYYY

## 测试情况
- [x] 所有单元测试通过
- [x] CI 检查通过
- [x] 代码审查通过
- [x] 性能验证通过

## 检查清单
- [x] 代码符合《华为 C++ 可信编码 CleanCode 规范》
- [x] 单元测试覆盖充分
- [x] 文档更新完整
- [x] 无遗留 TODO 和 FIXME
- [x] 无调试代码和临时代码
- [x] 向后兼容性确认

## 破坏性变更
无

## 部署建议
- 该功能为纯代码变更，无需特殊部署步骤
- 建议在下一个版本发布时包含此功能

## Code Review
请 @管理员 进行最终 Code Review

---

**功能状态**: ✅ 已完成，等待合入 master
**版本**: v9.0.0+
```

#### 步骤 4: 等待 Code Review 和合入

- 提交 PR 后，通知管理员和相关人员进行 Code Review
- 根据 Review 意见修改代码
- CI 检查通过后，等待管理员合入
- 合入后，PR 会自动关闭

#### 步骤 5: 清理分支（可选）

```bash
# PR 合入后，可以删除 feature 分支
git branch -d feature/oxc-ranktable
git push origin --delete feature/oxc-ranktable
```

### 3.5 实战案例：OXC rank table 功能开发

以下是完整的开发流程记录，供参考：

**创建分支**:
```bash
git checkout -b personal/zhangzhanbang
git push -u origin personal/zhangzhanbang
git checkout -b feature/oxc-ranktable
git push -u origin feature/oxc-ranktable
```

**提交代码**:
```bash
git checkout personal/zhangzhanbang
git add src/framework/common/src/topo/topoinfo_ranktableOxc.cc
git commit -m "feat(topo): 添加 OXC rank table 解析支持"
git push origin personal/zhangzhanbang
```

**创建 personal → feature PR**:
```bash
curl -X POST "https://gitcode.com/api/v5/repos/qq_30335613/8k_oxc_hcomm/pulls" \
  -H "Authorization: Bearer <token>" \
  -d '{
    "title": "feat: 添加 OXC rank table 解析支持",
    "head": "personal/zhangzhanbang",
    "base": "feature/oxc-ranktable",
    "body": "...",
    "remove_source_branch": false
  }'
```

**PR 链接**: https://gitcode.com/qq_30335613/8k_oxc_hcomm/merge_requests/1

**下一步**: 等待功能完整后，再提 feature → master 的 PR

---

### 3.6 向 master 提交 PR（旧流程，不推荐）

> **注意**: 以下为旧的单层 PR 流程，仅供参考。新项目请使用上面的两层 PR 工作流。

### 3.3 处理 PR 冲突

如果在 PR 过程中，master 有新的提交导致冲突：

```bash
# 1. 切换到 master，拉取最新代码
git checkout master
git pull origin master

# 2. 切换到个人分支
git checkout personal/aircat

# 3. 合并 master 的最新代码
git merge master

# 4. 解决冲突
# Git 会标记冲突文件，编辑这些文件：
# <<<<<<< HEAD
# 你的修改
# =======
# master 的修改
# >>>>>>> master

# 保留需要的代码，删除冲突标记后：
git add <解决冲突的文件>

# 5. 完成合并
git commit -m "resolve: 合并 master 最新代码，解决冲突"

# 6. 推送到远程，PR 会自动更新
git push origin personal/aircat
```

---

## 4. 常见开发场景

### 4.1 并行开发多个功能

```bash
# 基础：personal/aircat 分支（最新）

# 开始功能1
git checkout personal/aircat
git checkout -b feature/oxc-support
# ... 开发功能1 ...
git checkout personal/aircat
git merge feature/oxc-support

# 同时开始功能2
git checkout personal/aircat
git checkout -b feature/topo-fix
# ... 开发功能2 ...
git checkout personal/aircat
git merge feature/topo-fix

# 此时 personal/aircat 包含了两个功能
# 可以选择一次性提交 PR，或者分别提交
```

### 4.2 Bug 修复（紧急）

```bash
# 1. 从 master 创建修复分支
git checkout master
git pull origin master
git checkout -b fix/critical-bug-12345

# 2. 快速修复
vim src/framework/common/src/topo/topoinfo_ranktableOxc.cc
git add src/framework/common/src/topo/topoinfo_ranktableOxc.cc
git commit -m "fix: 修复 OXC 拓扑解析空指针崩溃问题

该问题会导致特定 rank table 配置下程序崩溃。

Fixes: #HCCL-5678"

# 3. 推送并创建紧急 PR
git push -u origin fix/critical-bug-12345

# 4. 通知管理员紧急处理
```

### 4.3 代码重构

```bash
# 1. 从个人分支创建重构分支
git checkout personal/aircat
git checkout -b refactor/topo-common-code

# 2. 进行重构
# - 提取公共函数
# - 重命名变量
# - 优化代码结构

# 3. 分阶段提交
git add src/framework/common/src/topo/topo_common.cc
git commit -m "refactor: 提取拓扑解析公共函数

- 提取 ParseTopologyLevel 为独立函数
- 减少代码重复

Refactoring: #HCCL-9876"

# 4. 确保所有测试通过
bash hcomm_compile.sh ut

# 5. 合并回个人分支
git checkout personal/aircat
git merge refactor/topo-common-code
git push origin personal/aircat
```

### 4.4 同步他人的代码

当你需要使用其他团队成员开发的功能时：

```bash
# 1. 添加对方的个人分支为远程分支（临时）
git fetch origin personal/zhangsan:personal/zhangsan

# 2. 查看对方的分支
git checkout personal/zhangsan
git log --oneline -10

# 3. 如果需要使用对方的代码，选择以下方式之一：

# 方式A: 合并到你的个人分支
git checkout personal/aircat
git merge personal/zhangsan

# 方式B: cherry-pick 特定提交
git checkout personal/aircat
git cherry-pick <commit-hash>

# 方式C: 基于对方的功能分支继续开发
git checkout personal/zhangsan
git checkout -b feature/extend-zhangsan-feature
```

---

## 5. 分支命名规范

### 5.1 命名格式

| 类型 | 格式 | 示例 |
|------|------|------|
| 个人分支 | `personal/<username>` | `personal/aircat` |
| 功能分支 | `feature/<module>-<description>` | `feature/topo-oxc-support` |
| Bug 修复 | `fix/<module>-<description>` | `fix/topo-parser-crash` |
| 重构 | `refactor/<module>-<description>` | `refactor/channel-manager` |
| 文档 | `docs/<description>` | `docs/api-reference` |
| 测试 | `test/<module>-<description>` | `test/topo-unit-test` |
| 性能优化 | `perf/<module>-<description>` | `perf-reduce-copy` |

### 5.2 命名约定

- 使用小写字母
- 使用连字符 `-` 分隔单词
- 描述要简明扼要（不超过 50 字符）
- 模块名可选，用于大型项目
- 避免使用中文和特殊字符

### 5.3 提交信息规范

使用**约定式提交**（Conventional Commits）格式：

```
<type>(<scope>): <subject>

<body>

<footer>
```

#### Type 类型

| Type | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat(topo): 添加 OXC 拓扑支持` |
| `fix` | Bug 修复 | `fix(parser): 修复 rank table 解析空指针` |
| `docs` | 文档更新 | `docs: 更新 API 使用说明` |
| `style` | 代码格式（不影响逻辑） | `style: 统一代码缩进为 4 空格` |
| `refactor` | 重构（不是新功能也不是修复） | `refactor(channel): 重构通道管理类` |
| `perf` | 性能优化 | `perf(reduce): 减少不必要的内存拷贝` |
| `test` | 测试相关 | `test(topo): 添加拓扑解析单元测试` |
| `chore` | 构建/工具相关 | `chore(cmake): 更新编译选项` |
| `revert` | 回退提交 | `revert: 回退 commit abc123` |

#### Subject 主题

- 使用中文描述（团队规范）
- 不超过 50 字符
- 首字母不大写
- 结尾不加句号
- 使用祈使句："添加" 而非 "添加了"

#### Body 正文（可选）

- 详细描述本次提交的内容
- 说明修改的原因和背景
- 列出主要改动点

#### Footer 脚注（可选）

- 关联 Issue: `Closes #HCCL-1234`
- 关联 PR: `Refs: PR #456`
- 破坏性变更: `BREAKING CHANGE: API 变更说明`

#### 完整示例

```bash
git commit -m "feat(topo): 添加 OXC 拓扑结构解析支持

- 实现 OXC 拓扑类型识别逻辑
- 支持 OXC rank table 格式解析
- 添加拓扑层级映射关系
- 更新单元测试用例，覆盖率 100%

该功能支持 A5 场景下的 OXC 组网配置，能够正确解析
包含 OXC 交换机的 rank table 文件。

Refs: #HCCL-1234
Co-Authored-By: 张三 <zhangsan@example.com>"
```

---

## 6. 代码评审（Code Review）规范

### 6.1 PR 创建者职责

- ✅ 确保代码通过编译和本地测试
- ✅ 填写完整的 PR 描述
- ✅ 提供清晰的变更说明
- ✅ 自查代码质量（运行静态检查工具）
- ✅ 响应 Reviewer 的意见
- ✅ 及时修改和更新 PR

### 6.2 Reviewer 职责

- ✅ 在 24 小时内完成 Review
- ✅ 重点关注代码逻辑正确性
- ✅ 检查是否符合编码规范
- ✅ 提供建设性的反馈意见
- ✅ 使用友好的评审语言

### 6.3 评审检查项

#### 功能性
- [ ] 代码逻辑正确
- [ ] 边界条件处理完善
- [ ] 错误处理到位
- [ ] 没有引入新的 Bug

#### 代码质量
- [ ] 符合《华为 C++ 可信编码 CleanCode 规范》
- [ ] 函数职责单一，长度适中
- [ ] 变量命名清晰
- [ ] 没有代码重复

#### 安全性
- [ ] 无缓冲区溢出风险
- [ ] 无内存泄漏风险
- [ ] 输入参数校验完善
- [ ] 使用安全的字符串函数

#### 可维护性
- [ ] 注释清晰到位
- [ ] 代码结构清晰
- [ ] 易于理解和扩展
- [ ] 模块间耦合度低

#### 测试
- [ ] 单元测试充分
- [ ] 测试覆盖主要场景
- [ ] 测试用例有效

### 6.4 评审结果

- **LGTM** (Looks Good To Me): 同意合入
- **Request Changes**: 需要修改后再 Review
- **Comment**: 仅提供建议，不阻塞合入
- **Approved**: 批准合入

---

## 7. CI/CD 集成

### 7.1 自动化检查

PR 创建后，CI 会自动执行以下检查：

- ✅ 编译检查：确保代码可以成功编译
- ✅ 静态检查：运行静态代码分析工具
- ✅ 单元测试：运行相关单元测试
- ✅ 代码覆盖率：检查测试覆盖率
- ✅ 编码规范：检查是否符合编码规范

### 7.2 CI 失败处理

如果 CI 检查失败：

1. 查看失败的详细日志
2. 在本地复现问题
3. 修复问题后推送新的 commit
4. CI 会自动重新运行检查

```bash
# 本地复现编译问题
cd /data/ccl_workspace/toolsh_dir
bash hcomm_compile.sh

# 本地运行单元测试
bash hcomm_compile.sh ut
```

### 7.3 跳过 CI（特殊情况）

仅在以下特殊情况下可以跳过 CI：
- 文档更新
- 注释修改
- 不影响代码逻辑的格式调整

跳过方式：
```bash
git commit -m "docs: 更新 README

[skip ci]"
```

---

## 8. 最佳实践

### 8.1 提交频率

- ✅ **频繁提交**: 小步快跑，每次提交一个完整的功能点
- ✅ **原子提交**: 每次提交只做一件事，逻辑独立
- ❌ **避免巨大提交**: 一次提交包含多个不相关的改动

### 8.2 提交粒度

```bash
# ❌ 不好：一次提交包含多个改动
git commit -m "很多修改"

# ✅ 好：拆分为多个独立提交
git commit -m "feat(topo): 添加 OXC 拓扑识别"
git commit -m "test(topo): 添加 OXC 单元测试"
git commit -m "docs: 更新拓扑模块文档"
```

### 8.3 分支管理

- ✅ **及时删除已合并的分支**: 保持分支列表清晰
- ✅ **定期同步 master**: 减少冲突概率
- ✅ **分支名称有意义**: 一目了然

```bash
# 查看已合并的本地分支
git branch --merged

# 删除已合并的本地分支
git branch -d feature/oxc-support

# 查看已合并的远程分支
git branch -r --merged

# 删除已合并的远程分支
git push origin --delete feature/oxc-support
```

### 8.4 代码自检清单

提交 PR 前请确认：

- [ ] 代码已通过本地编译
- [ ] 单元测试全部通过
- [ ] 代码符合编码规范
- [ ] 已添加必要的注释
- [ ] 更新了相关文档
- [ ] 没有调试代码和临时代码
- [ ] 没有 TODO 和 FIXME（或已创建 Issue）
- [ ] 已执行 `bash hcomm_compile.sh` 编译验证

### 8.5 冲突预防

```bash
# 开发前：拉取最新代码
git checkout personal/aircat
git pull origin personal/aircat

# 开发中：定期同步
git fetch origin
git rebase origin/personal/aircat

# 提交 PR 前：确保是最新的
git pull --rebase origin personal/aircat
git push origin personal/aircat
```

### 8.6 历史记录保持清晰

```bash
# 使用 --no-ff 合并，保留分支历史
git merge --no-ff feature/oxc-support

# 使用 rebase 整理提交历史（个人分支上）
git rebase -i HEAD~3

# 注意：不要对已推送的公开分支执行 rebase
```

---

## 9. 团队约定

### 9.1 开发时间

- **日常开发**: 在个人 `personal/<username>` 分支上进行
- **紧急修复**: 可以从 master 直接创建 `fix/*` 分支
- **大型功能**: 建议先在项目内部讨论，避免方向错误

### 9.2 PR 合并时机

- **普通功能**: 由管理员安排时间合入
- **紧急修复**: 优先处理，立即合入
- **争议性改动**: 先在团队内部讨论达成一致

### 9.3 分支保护规则

| 分支 | 保护规则 |
|------|---------|
| `master` | ✅ 禁止直接推送<br>✅ 必须通过 PR 合入<br>✅ 需要 Code Review<br>✅ 需要 CI 通过<br>✅ 管理员批准 |
| `personal/*` | ⚠️ 所有者可直接推送<br>✅ 其他人需要 PR |

### 9.4 用户名规范

为了保持一致性，建议使用统一的用户名格式：

- 格式：全拼小写或常用缩写
- 示例：`zhangsan`、`lisi`、`wangwu`、`aircat`
- 避免使用：临时名称、特殊字符、中文拼音

---

## 10. 常见问题（FAQ）

### Q1: 我不小心提交到了 master 分支怎么办？

```bash
# 如果还没推送到远程
git reset --soft HEAD~1  # 撤销最后一次提交，保留修改
git checkout personal/aircat
git commit -m "正确的提交信息"

# 如果已经推送到远程
# 立即联系管理员处理
# 可能需要强制回退（谨慎操作）
```

### Q2: 我的 PR 和其他人的 PR 冲突了怎么办？

```bash
# 1. 拉取最新的 master
git fetch origin master
git checkout master
git pull origin master

# 2. 在你的分支上合并 master
git checkout personal/aircat
git merge master

# 3. 解决冲突
# ... 编辑冲突文件 ...
git add <解决冲突的文件>
git commit

# 4. 推送更新
git push origin personal/aircat
# PR 会自动更新
```

### Q3: 如何撤销已经推送的 commit？

```bash
# 情况1: 最新的几个 commit 需要撤销（未合入 master）
git reset --hard HEAD~3  # 撤销最近 3 个 commit
git push -f origin personal/aircat  # 强制推送（谨慎！）

# 情况2: 需要保留历史，只是回退代码
git revert <commit-hash>
git push origin personal/aircat

# 注意：永远不要对 master 执行强制推送
```

### Q4: 我想使用其他人的功能，但还没有合入 master 怎么办？

```bash
# 方式1: fetch 对方的分支
git fetch origin personal/zhangsan
git checkout personal/zhangsan
git checkout -b feature/based-on-zhangsan-work

# 方式2: cherry-pick 特定提交
git log personal/zhangsan  # 找到需要的 commit hash
git cherry-pick <commit-hash>
```

### Q5: 个人分支需要从 master 重新创建吗？

不需要。个人分支是长期维护的，只需要定期同步 master 的最新代码：

```bash
git checkout personal/aircat
git fetch origin master
git merge origin/master
git push origin personal/aircat
```

### Q6: 多人协作同一个功能怎么办？

```bash
# 方式1: 使用共享的功能分支
# 开发者A创建功能分支
git checkout -b feature/joint-development
git push -u origin feature/joint-development

# 开发者B checkout 同一个分支
git fetch origin
git checkout feature/joint-development
# ... 进行开发 ...

# 方式2: 各自开发，最后合并
# 开发者A: personal/dev-a -> feature/part-a
# 开发者B: personal/dev-b -> feature/part-b
# 最后统一合并到一个分支
```

---

## 11. 附录

### 11.1 有用的 Git 命令

```bash
# 查看分支状态
git status

# 查看分支列表
git branch -a  # 所有分支（本地+远程）
git branch -r  # 仅远程分支
git branch --merged  # 已合并的分支

# 查看提交历史
git log --oneline --graph --all  # 图形化显示
git log --author="aircat"  # 某个作者的提交
git log --since="2 weeks ago"  # 最近两周的提交

# 查看差异
git diff  # 工作区和暂存区差异
git diff --cached  # 暂存区和 HEAD 差异
git diff personal/aircat master  # 两个分支差异

# 暂存和恢复
git stash  # 暂存当前修改
git stash pop  # 恢复暂存的修改
git stash list  # 查看暂存列表

# 清理
git clean -fd  # 清理未跟踪的文件和目录
git gc  # 垃圾回收，优化仓库

# 查看配置
git config --list
git config user.name
git remote -v
```

### 11.2 参考资料

- [Git 官方文档](https://git-scm.com/doc)
- [Pro Git 中文版](https://git-scm.com/book/zh/v2)
- [约定式提交规范](https://www.conventionalcommits.org/zh-hans/)
- [GitHub Flow](https://docs.github.com/en/get-started/using-github/github-flow)
- [Git Flow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow)

### 11.3 团队联系

| 角色 | 姓名 | 邮箱 | 职责 |
|------|------|------|------|
| 项目管理员 | - | - | master 分支管理、PR 合入 |
| 技术负责人 | - | - | 技术方案审核、Code Review |
| CI 维护者 | - | - | CI/CD 配置和维护 |

### 11.4 变更记录

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| v1.0 | 2026-02-11 | aircat | 初始版本 |
| v1.1 | 2026-02-11 | zhangzhanbang | 更新为两层 PR 工作流（personal → feature → master）；添加 GitCode API 使用说明；添加实战案例 |

---

## 12. 总结

本工作流的核心思想：

1. **master 受保护**: 确保主线始终稳定
2. **个人分支自由**: 开发者有自己的工作空间
3. **功能分支隔离**: 每个功能独立开发，易于管理
4. **代码评审质量**: 所有合入必须经过 Review
5. **自动化检查**: CI 确保代码质量

遵循本规范，团队可以：
- ✅ 提高协作效率
- ✅ 减少代码冲突
- ✅ 保证代码质量
- ✅ 保持清晰的提交历史
- ✅ 快速定位和修复问题

---

**祝开发愉快！** 🚀
