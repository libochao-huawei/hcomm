# log_extract.py

用于从 Ascend/HCCL 日志目录中提取目标内容，适合快速筛取：
- ranktable 解析日志
- OXC 1.4 / A3 日志
- version=1.2 / superPod 日志
- netPlane / RankGraph 侧证日志

## 用法

```bash
python3 scripts/log_extract.py <日志根目录> --preset ranktable
```

## 常用示例

### 1. 提取通用 ranktable 解析信息
```bash
python3 scripts/log_extract.py /path/to/log --preset ranktable
```

### 2. 提取 OXC/A3 相关日志
```bash
python3 scripts/log_extract.py /path/to/log --preset ranktable-oxc --areas run debug
```

### 3. 提取 version=1.2 / superPod 日志
```bash
python3 scripts/log_extract.py /path/to/log --preset ranktable-v12 --areas debug
```

### 4. 追加自定义关键字，并带 1 行上下文
```bash
python3 scripts/log_extract.py /path/to/log --preset ranktable \
  --pattern "serverIdx" --pattern "superPodId" --context 1
```

### 5. 以 JSON 输出并保存结果
```bash
python3 scripts/log_extract.py /path/to/log --preset ranktable-oxc \
  --format json --save /tmp/ranktable_extract.json
```

## 说明
- 默认扫描一级目录：`run debug security`
- 默认会跳过二进制文件和超大文件
- `ranktable-oxc` 适合筛 OXC 新增的 `Summary/Detail` 日志
- `ranktable-v12` 适合筛 `TopoinfoRanktableConcise` 的 `super_pod` / `superDevice` 相关日志
