#!/bin/bash
# 获取文件路径
file_path="$1"
output_file="$2"

# 生成 sha256sum
hash_output=$(sha256sum "$file_path")

#提取 hash和文件名
hash=$(echo "$hash_output" | awk '{print $1}')
filename=$(basename "$file_path")

#生成目标文件
echo "${filename}=${hash}" > "$output_file"