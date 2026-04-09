#!/bin/bash
set -e

json_escape() {
    local s="$1"
    s="${s//\\/\\\\}"
    s="${s//\"/\\\"}"
    echo "$s"
}

parse_ini() {
    local file="$1"
    local -n _ops=$2
    
    local current_op=""
    
    while IFS= read -r line || [[ -n "$line" ]]; do
        # 跳过注释和空行
        [[ "$line" =~ ^[[:space:]]*#.*$ ]] && continue
        [[ "$line" =~ ^[[:space:]]*$ ]] && continue
        
        # 解析 [Section]
        if [[ "$line" =~ ^\[([^]]+)\]$ ]]; then
            current_op="${BASH_REMATCH[1]}"
            _ops["$current_op"]=""
            continue
        fi
        
        # 解析 key = value
        if [[ "$line" =~ ^([^=]+)=(.*)$ ]] && [[ -n "$current_op" ]]; then
            local key="${BASH_REMATCH[1]}"
            local val="${BASH_REMATCH[2]}"
            # 去除首尾空格
            key=$(echo "$key" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
            val=$(echo "$val" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
            
            # 解析 section.subsection
            if [[ "$key" =~ ^([^.]+)\\.(.+)$ ]]; then
                local sec="${BASH_REMATCH[1]}"
                local subsec="${BASH_REMATCH[2]}"
                _ops["$current_op"]+="${_ops[$current_op]:+,}$sec:$subsec:$(json_escape "$val")"
            fi
        fi
    done < "$file"
}

generate_json() {
    local -n _ops=$1
    local first=1
    
    echo "{"
    for op in "${!_ops[@]}"; do
        [[ $first -eq 0 ]] && echo ","
        first=0
        
        echo "    \"$op\": {"
        
        local pairs="${_ops[$op]}"
        local -A sections
        local sec_order=()
        
        # 按section分组
        IFS=',' read -ra entries <<< "$pairs"
        for entry in "${entries[@]}"; do
            [[ -z "$entry" ]] && continue
            IFS=':' read -r sec subsec val <<< "$entry"
            
            if [[ -z "${sections[$sec]}" ]]; then
                sec_order+=("$sec")
            fi
            sections["$sec"]+="${sections[$sec]:+,}$subsec:$val"
        done
        
        # 输出sections
        local first_sec=1
        for sec in "${sec_order[@]}"; do
            [[ $first_sec -eq 0 ]] && echo ","
            first_sec=0
            
            echo "        \"$sec\": {"
            
            local sub_pairs="${sections[$sec]}"
            local first_sub=1
            
            IFS=',' read -ra subs <<< "$sub_pairs"
            for sub in "${subs[@]}"; do
                [[ -z "$sub" ]] && continue
                IFS=':' read -r sub_key sub_val <<< "$sub"
                
                [[ $first_sub -eq 0 ]] && echo ","
                first_sub=0
                printf "            \"%s\": \"%s\"" "$sub_key" "$sub_val"
            done
            echo ""
            echo -n "        }"
        done
        echo ""
        echo -n "    }"
    done
    echo ""
    echo "}"
}

# 主逻辑
main() {
    [[ $# -lt 1 ]] && { echo "Usage: $0 <ini_file> [output.json]"; exit 1; }
    
    local ini_file="$1"
    local out_file="${2:-${ini_file%.ini}.json}"
    
    [[ ! -f "$ini_file" ]] && { echo "Error: $ini_file not found"; exit 1; }
    
    declare -A ops
    parse_ini "$ini_file" ops
    generate_json ops > "$out_file"
    
    chmod 640 "$out_file"
    echo "Generated: $out_file ($((${#ops[@]})) ops)"
}

main "$@"