#!/bin/sh
# ----------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------

mk_custom_path() {
    if [ $(id -u) -eq 0 ]; then
        return 0
    fi
    local _custom_path_file="$1"
    while read line || [ -n "$line" ]
    do
        local _custom_path="$(echo "$line" | cut --only-delimited -d= -f2)"
        if [ -z "$_custom_path" ]; then
            continue
        fi
        eval "_custom_path=$_custom_path"
        if [ ! -d "$_custom_path" ]; then
            mkdir -p "$_custom_path"
            if [ $? -ne 0 ]; then
                cur_date="$(date +"%Y-%m-%d %H:%M:%S")"
                echo "[Common] [$cur_date] [ERROR]: create $_custom_path failed."
                return 1
            fi
        fi
    done < $_custom_path_file
    return 0
}