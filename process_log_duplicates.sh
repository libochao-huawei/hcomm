#!/bin/bash

# log文件处理脚本
# 功能：删除"TEST2"前的字段，然后查找重复行

# 使用说明
usage() {
    echo "用法: $0 <input_log_file> [output_file]"
    echo ""
    echo "参数说明:"
    echo "  input_log_file  输入的log文件路径"
    echo "  output_file     可选，输出文件路径（默认为input_log_file.processed）"
    echo ""
    echo "示例:"
    echo "  $0 input.log"
    echo "  $0 input.log output.log"
    echo ""
    echo "功能说明:"
    echo "  1. 读取输入文件，删除每行中'TEST2'之前的字段"
    echo "  2. 将处理后的内容保存到中间文件"
    echo "  3. 查找中间文件中的重复行并打印"
    exit 1
}

# 检查参数
if [ $# -lt 1 ]; then
    usage
fi

INPUT_FILE="$1"
TEMP_FILE="${INPUT_FILE}.processed"

# 检查输入文件是否存在
if [ ! -f "$INPUT_FILE" ]; then
    echo "错误: 输入文件 '$INPUT_FILE' 不存在"
    exit 1
fi

echo "=== 处理文件: $INPUT_FILE ==="
echo "=== 第一步: 删除'TEST2'前的字段 ==="

# 第一步：删除"TEST2"前的字段
# 使用sed删除每行中TEST2之前的所有内容（包括TEST2）
sed 's/.*TEST2//g' "$INPUT_FILE" > "$TEMP_FILE"

# 检查处理是否成功
if [ $? -ne 0 ]; then
    echo "错误: 处理文件失败"
    exit 1
fi

echo "=== 中间文件已生成: $TEMP_FILE ==="
echo "=== 第二步: 查找重复行 ==="

# 第二步：查找重复行
# 方法1: 使用awk查找重复行
echo ""
echo "=== 重复行列表 (使用awk) ==="
awk '
{
    # 记录每行出现的次数
    count[$0]++
}
END {
    # 打印出现次数大于1的行
    for (line in count) {
        if (count[line] > 1) {
            print "重复行: [" line "] 出现次数: " count[line]
        }
    }
}
' "$TEMP_FILE"

# 方法2: 使用sort和uniq查找重复行
echo ""
echo "=== 重复行列表 (使用sort+uniq) ==="
sort "$TEMP_FILE" | uniq -d | while read line; do
    count=$(grep -c "$line" "$TEMP_FILE")
    echo "重复行: [$line] 出现次数: $count"
done

# 方法3: 使用纯bash查找重复行（显示重复行的行号）
echo ""
echo "=== 重复行详细信息 (显示行号) ==="

declare -A line_count
declare -A line_numbers
line_num=0

# 读取中间文件，统计每行出现的次数和行号
while IFS= read -r line; do
    line_num=$((line_num + 1))
    
    # 记录行号
    if [ -n "${line_numbers[$line]}" ]; then
        line_numbers[$line]="${line_numbers[$line]},$line_num"
    else
        line_numbers[$line]="$line_num"
    fi
    
    # 统计出现次数
    line_count[$line]=$((${line_count[$line]:-0} + 1))
done < "$TEMP_FILE"

# 打印重复行
found_duplicates=0
for line in "${!line_count[@]}"; do
    count=${line_count[$line]}
    if [ $count -gt 1 ]; then
        found_duplicates=1
        echo "重复行: [$line]"
        echo "  出现次数: $count"
        echo "  行号: ${line_numbers[$line]}"
        echo ""
    fi
done

if [ $found_duplicates -eq 0 ]; then
    echo "未发现重复行"
fi

# 统计信息
echo ""
echo "=== 统计信息 ==="
total_lines=$(wc -l < "$TEMP_FILE")
unique_lines=$(sort "$TEMP_FILE" | uniq | wc -l)
duplicate_lines=$((total_lines - unique_lines))

echo "总行数: $total_lines"
echo "唯一行数: $unique_lines"
echo "重复行数: $duplicate_lines"
echo "中间文件: $TEMP_FILE"

echo ""
echo "=== 处理完成 ==="

# 询问是否删除中间文件
read -p "是否删除中间文件 '$TEMP_FILE'? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm "$TEMP_FILE"
    echo "中间文件已删除"
else
    echo "中间文件保留: $TEMP_FILE"
fi

exit 0