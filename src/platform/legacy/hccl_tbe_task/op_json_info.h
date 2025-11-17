/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef OP_JSON_INFO_H
#define OP_JSON_INFO_H

#include <nlohmann/json.hpp>

static nlohmann::json dynamic_add_float16_v51 = {
    {"binFileName", "dynamic_add_float16_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_add_float16_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "1d20127cf030155737aefc9a3fd24b26011956d644c611241ca36180cc58ee60"}
};

static nlohmann::json dynamic_add_float16_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 2},
    {"_coexisting_quantity", 3},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {{"210000000", {100, 200, 300}},
                       {"312000000", {101, 200, 300}},
                       {"321000000", {100, 200, 300}}}},
    {"_vars", {{"210000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}},
               {"312000000", {"dim_0_1", "block_factor_0", "ub_factor_0"}},
               {"321000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}}}}
};

static nlohmann::json dynamic_add_float32_v51 = {
    {"binFileName", "dynamic_add_float32_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_add_float32_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "0061e4cb079ef31d18a6f39f758a58ea984385f71661cd0db9d5ef32d1e7917b"}
};

static nlohmann::json dynamic_add_float32_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 3},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {{"210000000", {100, 200, 300}},
                       {"312000000", {101, 200, 300}},
                       {"321000000", {100, 200, 300}}}},
    {"_vars", {{"210000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}},
               {"312000000", {"dim_0_1", "block_factor_0", "ub_factor_0"}},
               {"321000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}}}}
};

static nlohmann::json dynamic_add_int32_v51 = {
    {"binFileName", "dynamic_add_int32_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_add_int32_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "a2773a53e37b7b0c2b596dfaae4c54b90bf3df26cae637d31f40061e693b3f64"}
};

static nlohmann::json dynamic_add_int32_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 3},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {{"210000000", {100, 200, 300}},
                       {"312000000", {101, 200, 300}},
                       {"321000000", {100, 200, 300}}}},
    {"_vars", {{"210000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}},
               {"312000000", {"dim_0_1", "block_factor_0", "ub_factor_0"}},
               {"321000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}}}}
};

static nlohmann::json dynamic_add_float16_v80 = {
    {"binFileName", "add_Ascend910_float16"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "add_Ascend910_float16_210000000"},
                    {"kernelName", "add_Ascend910_float16_210010000"},
                    {"kernelName", "add_Ascend910_float16_232000000"},
                    {"kernelName", "add_Ascend910_float16_232010000"},
                    {"kernelName", "add_Ascend910_float16_223000000"},
                    {"kernelName", "add_Ascend910_float16_223010000"}}},
    {"kernelName", "add_Ascend910_float16"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "122e9f58a7a82b4c8801f76ef9206337f46c03265443b72b131ad1d486adcee7"}
};

static nlohmann::json dynamic_add_float16_v80_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_max_dtype_bytes", 2},
    {"_base_info", {{"100", {32, 2, 43680, 21840}},
                    {"320", {32, 2, 43664, 21824}},
                    {"230", {32, 2, 43664, 21824}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_add_float32_v80 = {
    {"binFileName", "add_Ascend910_float32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "add_Ascend910_float32_210000000"},
                    {"kernelName", "add_Ascend910_float32_210010000"},
                    {"kernelName", "add_Ascend910_float32_232000000"},
                    {"kernelName", "add_Ascend910_float32_232010000"},
                    {"kernelName", "add_Ascend910_float32_223000000"},
                    {"kernelName", "add_Ascend910_float32_223010000"}}},
    {"kernelName", "add_Ascend910_float32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "3cc41979b9e71d021f5ec876c24f3dbf10dfce088a958c1c44762ffa5ea8c4ba"}
};

static nlohmann::json dynamic_add_float32_v80_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_max_dtype_bytes", 2},
    {"_base_info", {{"100", {32, 4, 21840, 10920}},
                    {"320", {32, 4, 21832, 10912}},
                    {"230", {32, 4, 21832, 10912}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_add_int32_v80 = {
    {"binFileName", "dynamic_add_int32_v80"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_add_int32_v80__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "63b16dff9848e8bb60ae6b01c2c2c0dd333def39f9a70d76ceb69f2196e4a86b"}
};

static nlohmann::json dynamic_add_int32_v80_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 32},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {{"210000000", {100, 200, 300}},
                       {"312000000", {101, 200, 300}},
                       {"321000000", {100, 200, 300}}}},
    {"_vars", {{"210000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}},
               {"312000000", {"dim_0_1", "block_factor_0", "ub_factor_0"}},
               {"321000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}}}}
};

static nlohmann::json dynamic_mul_float16_v51 = {
    {"binFileName", "dynamic_mul_float16_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_mul_float16_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "a44c1b5a57af3d8f45948ed6a804852f632a03f74ae2131b0070d79893eca92f"}
};

static nlohmann::json dynamic_mul_float16_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 2},
    {"_coexisting_quantity", 3},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {{"210000000", {100, 200, 300}},
                       {"312000000", {101, 200, 300}},
                       {"321000000", {100, 200, 300}}}},
    {"_vars", {{"210000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}},
               {"312000000", {"dim_0_1", "block_factor_0", "ub_factor_0"}},
               {"321000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}}}}
};

static nlohmann::json dynamic_mul_float32_v51 = {
    {"binFileName", "dynamic_mul_float32_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_mul_float32_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "251fc1a0098f3fb6a876bc2b0aea0fe2f6e8a184e7ac32e9405170744dbc2280"}
};

static nlohmann::json dynamic_mul_float32_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 3},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {{"210000000", {100, 200, 300}},
                       {"312000000", {101, 200, 300}},
                       {"321000000", {100, 200, 300}}}},
    {"_vars", {{"210000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}},
               {"312000000", {"dim_0_1", "block_factor_0", "ub_factor_0"}},
               {"321000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}}}}
};

static nlohmann::json dynamic_mul_int32_v51 = {
    {"binFileName", "dynamic_add_float16_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_mul_int32_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "506dc6c5589f8b1afc6857b6a787688d0930412a196b82afeffede209592faed"}
};

static nlohmann::json dynamic_mul_int32_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 3},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {{"210000000", {100, 200, 300}},
                       {"312000000", {101, 200, 300}},
                       {"321000000", {100, 200, 300}}}},
    {"_vars", {{"210000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}},
               {"312000000", {"dim_0_1", "block_factor_0", "ub_factor_0"}},
               {"321000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}}}}
};

static nlohmann::json dynamic_mul_float16_v80 = {
    {"binFileName", "mul_Ascend910_float16"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910_float16_210000000"},
                    {"kernelName", "mul_Ascend910_float16_210010000"},
                    {"kernelName", "mul_Ascend910_float16_232000000"},
                    {"kernelName", "mul_Ascend910_float16_232010000"},
                    {"kernelName", "mul_Ascend910_float16_223000000"},
                    {"kernelName", "mul_Ascend910_float16_223010000"}}},
    {"kernelName", "mul_Ascend910_float16"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "133ffd0cd31e7082e3d8f4af91acb9078e1f7a525737b766ffac4911684bc1ca"}
};

static nlohmann::json dynamic_mul_float16_v80_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_max_dtype_bytes", 2},
    {"_base_info", {{"100", {32, 2, 43680, 21840}},
                    {"320", {32, 2, 43664, 21824}},
                    {"230", {32, 2, 43664, 21824}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_float32_v80 = {
    {"binFileName", "mul_Ascend910_float32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910_float32_210000000"},
                    {"kernelName", "mul_Ascend910_float32_210010000"},
                    {"kernelName", "mul_Ascend910_float32_232000000"},
                    {"kernelName", "mul_Ascend910_float32_232010000"},
                    {"kernelName", "mul_Ascend910_float32_223000000"},
                    {"kernelName", "mul_Ascend910_float32_223010000"}}},
    {"kernelName", "mul_Ascend910_float32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "ef0280537a78f7c4216b4f54a02a4d03618a2fcdb61e167eb4e0585ca29cfad2"}
};

static nlohmann::json dynamic_mul_float32_v80_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_max_dtype_bytes", 2},
    {"_base_info", {{"100", {32, 4, 21840, 10920}},
                    {"320", {32, 4, 21832, 10912}},
                    {"230", {32, 4, 21832, 10912}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_int32_v80 = {
    {"binFileName", "dynamic_mul_int32_v80"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_mul_int32_v80__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "15d374a11d2c62014707e2deb2d41faa0bacc400c050b6007c9fc09f8bceced8"}
};

static nlohmann::json dynamic_mul_int32_v80_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 32},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {{"210000000", {100, 200, 300}},
                       {"312000000", {101, 200, 300}},
                       {"321000000", {100, 200, 300}}}},
    {"_vars", {{"210000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}},
               {"312000000", {"dim_0_1", "block_factor_0", "ub_factor_0"}},
               {"321000000", {"dim_0_0", "block_factor_0", "ub_factor_0"}}}}
};

static nlohmann::json dynamic_maximum_float16_v51 = {
    {"binFileName", "dynamic_maximum_float16_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_maximum_float16_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "d386f3172d080a8191b021a3115a4bc5b28578fd02c9a0885aa56527e5d56aae"}
};

static nlohmann::json dynamic_maximum_float16_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 2},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_maximum_float32_v51 = {
    {"binFileName", "dynamic_maximum_float32_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_maximum_float32_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "53d917a815f9d842dd819c59a5c7138bf2e40ee39bddf1bc251496ae07a7a81b"}
};

static nlohmann::json dynamic_maximum_float32_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_maximum_int32_v51 = {
    {"binFileName", "dynamic_maximum_int32_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_maximum_int32_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "a0a9d78d59d67bcd307fccb702af030d9aafc5a1dcc44dec36b71f490c85e404"}
};

static nlohmann::json dynamic_maximum_int32_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_maximum_float16_v80 = {
    {"binFileName", "dynamic_maximum_float16_v80"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_maximum_float16_v80__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELf"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "fe29a4d52942255d8523ac257c174a444f681144e5ec29b8ab24dee59e6391d9"}
};

static nlohmann::json dynamic_maximum_float16_v80_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 2},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 32},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_maximum_float32_v80 = {
    {"binFileName", "dynamic_maximum_float32_v80"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_maximum_float32_v80__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "600d22d56b94b05a33caf183b12a6d708fbed9ebbd5cad1de95e5eba7be5a575"}
};

static nlohmann::json dynamic_maximum_float32_v80_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 32},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_maximum_int32_v80 = {
    {"binFileName", "dynamic_maximum_int32_v80"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_maximum_int32_v80__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "f3f801353d783b438172b01f71f1181263ff8bf16b803a118735b588be400fe9"}
};

static nlohmann::json dynamic_maximum_int32_v80_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 32},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_minimum_float16_v51 = {
    {"binFileName", "dynamic_minimum_float16_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_minimum_float16_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "749953df4cea03389dd2882bfe87771d186779864b540a0380e489ec074e6d53"}
};

static nlohmann::json dynamic_minimum_float16_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 2},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_minimum_float32_v51 = {
    {"binFileName", "dynamic_minimum_float32_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_minimum_float32_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "2a67e405ac83ab2004953d6aade034388b0c29ef14890512a714b9edfc6a3d81"}
};

static nlohmann::json dynamic_minimum_float32_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_minimum_int32_v51 = {
    {"binFileName", "dynamic_minimum_int32_v51"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_minimum_int32_v51__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "fee146189dd085c6b2715de8a951bc492e1fd87db4f2298ebad3d00601bd445a"}
};

static nlohmann::json dynamic_minimum_int32_v51_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 8},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_minimum_float16_v80 = {
    {"binFileName", "dynamic_minimum_float16_v80"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_minimum_float16_v80__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "c1663eff05ceac6d9d4d1a623785dc1d316cb436b5d57184261227b8673914da"}
};

static nlohmann::json dynamic_minimum_float16_v80_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 2},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 32},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_minimum_float32_v80 = {
    {"binFileName", "dynamic_minimum_float32_v80"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_minimum_float32_v80__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "09515a7079f9f757914baae179e4dcea4d8649ff7384777cef52fa9729499693"}
};

static nlohmann::json dynamic_minimum_float32_v80_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 32},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_minimum_int32_v80 = {
    {"binFileName", "dynamic_minimum_int32_v80"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelName", "dynamic_minimum_int32_v80__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 16},
    {"parameters", {
        0,
        0,
        0,
        0
    }},
    {"sha256", "a1dfd379f35c0eb11614e561331266afd3109f0d439137d094c56762f2016eb8"}
};

static nlohmann::json dynamic_minimum_int32_v80_tiling_info = {
    {"_fusion", 2},
    {"_pattern", "ElemWise"},
    {"_max_dtype_bytes", 4},
    {"_coexisting_quantity", 4},
    {"_ub_size", 262144},
    {"_core_num", 32},
    {"_is_support_broadcast", true},
    {"_is_const_shapes", false},
    {"_is_support_absorbable_broadcast", true},
    {"_use_special_pattern", true},
    {"_elewise_vars", {
        {"210000000", {
            100, 200, 300}},
        {"312000000", {
            101, 200, 300}},
        {"321000000", {
            100, 200, 300}}
    }},
    {"_vars", {
        {"210000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}},
        {"312000000", {
            "dim_0_1", "block_factor_0", "ub_factor_0"}},
        {"321000000", {
            "dim_0_0", "block_factor_0", "ub_factor_0"}}
    }}
};

static nlohmann::json dynamic_add_int8_v80 = {
    {"binFileName", "add_Ascend910_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "add_Ascend910_int8_210000000"},
                    {"kernelName", "add_Ascend910_int8_210010000"},
                    {"kernelName", "add_Ascend910_int8_232000000"},
                    {"kernelName", "add_Ascend910_int8_223000000"}}},
    {"kernelName", "add_Ascend910_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "860046a1f1dd13eaef907c7ad913761aa838c554e980a9a2eff0d71e94b1271a"}
};

static nlohmann::json dynamic_add_int8_v80_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "Broadcast"},
    {"_outs_uint1", false},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_base_info", {{"100", {32, 4, 16352, 8176}},
                    {"320", {32, 4, 15864, 7928}},
                    {"230", {32, 4, 15864, 7928}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_attr_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}}}}
};

static nlohmann::json dynamic_add_int8_v51 = {
    {"binFileName", "add_Ascend310P3_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "add_Ascend310P3_int8_210000000"},
                    {"kernelName", "add_Ascend310P3_int8_210010000"},
                    {"kernelName", "add_Ascend310P3_int8_232000000"},
                    {"kernelName", "add_Ascend310P3_int8_223000000"}}},
    {"kernelName", "add_Ascend310P3_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "6118afa7be0837d917d458545ac835ddafb2d690cafc5b5d26fdf8cb90e98231"}
};

static nlohmann::json dynamic_add_int8_v51_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "Broadcast"},
    {"_outs_uint1", false},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_base_info", {{"100", {8, 4, 16360, 8176}},
                    {"320", {8, 4, 15872, 7936}},
                    {"230", {8, 4, 15872, 7936}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_attr_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}}}}
};

static nlohmann::json dynamic_mul_int8_v80 = {
    {"binFileName", "mul_Ascend910_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910_int8_210000000"},
                    {"kernelName", "mul_Ascend910_int8_210010000"},
                    {"kernelName", "mul_Ascend910_int8_232000000"},
                    {"kernelName", "mul_Ascend910_int8_223000000"}}},
    {"kernelName", "mul_Ascend910_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "ec63002ef280c034d572430faa8b1f8b5ec060aa8ae5d994cb53b09f138bef23"}
};

static nlohmann::json dynamic_mul_int8_v80_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "Broadcast"},
    {"_outs_uint1", false},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_base_info", {{"100", {32, 4, 16344, 8168}},
                    {"320", {32, 4, 16376, 8184}},
                    {"230", {32, 4, 16376, 8184}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_attr_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}}}}
};

static nlohmann::json dynamic_mul_int8_v51 = {
    {"binFileName", "mul_Ascend310P3_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend310P3_int8_210000000"},
                    {"kernelName", "mul_Ascend310P3_int8_210010000"},
                    {"kernelName", "mul_Ascend310P3_int8_232000000"},
                    {"kernelName", "mul_Ascend310P3_int8_223000000"}}},
    {"kernelName", "mul_Ascend310P3_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "3fa88e10fbdfbb55e0d820b8a9bd43c487545c2267ce6562f9b44875c32fe9ad"}
};

static nlohmann::json dynamic_mul_int8_v51_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "Broadcast"},
    {"_outs_uint1", false},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_base_info", {{"100", {8, 4, 16344, 8168}},
                    {"320", {8, 4, 16384, 8192}},
                    {"230", {8, 4, 16384, 8192}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_attr_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}}}}
};

static nlohmann::json dynamic_maximum_int8_v80 = {
    {"binFileName", "maximum_Ascend910_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "maximum_Ascend910_int8_210000000"},
                    {"kernelName", "maximum_Ascend910_int8_210010000"},
                    {"kernelName", "maximum_Ascend910_int8_232000000"},
                    {"kernelName", "maximum_Ascend910_int8_223000000"}}},
    {"kernelName", "maximum_Ascend910_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "63dd842a9d4b34b28444c1e171622bec8d26753d22e60767a3bb3f7ce0b55eea"}
};

static nlohmann::json dynamic_maximum_int8_v80_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "Broadcast"},
    {"_outs_uint1", false},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_base_info", {{"100", {32, 2, 32736, 16368}},
                    {"320", {32, 2, 42320, 21152}},
                    {"230", {32, 2, 42320, 21152}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_attr_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}}}}
};

static nlohmann::json dynamic_maximum_int8_v51 = {
    {"binFileName", "maximum_Ascend310P3_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "maximum_Ascend310P3_int8_210000000"},
                    {"kernelName", "maximum_Ascend310P3_int8_210010000"},
                    {"kernelName", "maximum_Ascend310P3_int8_232000000"},
                    {"kernelName", "maximum_Ascend310P3_int8_223000000"}}},
    {"kernelName", "maximum_Ascend310P3_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "a9122848d649f68b0ca425cdd78f07576734c61612fa915949ca351297a106a3"}
};

static nlohmann::json dynamic_maximum_int8_v51_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "Broadcast"},
    {"_outs_uint1", false},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_base_info", {{"100", {8, 2, 32736, 16368}},
                    {"320", {8, 2, 42320, 21152}},
                    {"230", {8, 2, 42320, 21152}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_attr_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}}}}
};

static nlohmann::json dynamic_minimum_int8_v80 = {
    {"binFileName", "minimum_Ascend910_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "minimum_Ascend910_int8_210000000"},
                    {"kernelName", "minimum_Ascend910_int8_210010000"},
                    {"kernelName", "minimum_Ascend910_int8_232000000"},
                    {"kernelName", "minimum_Ascend910_int8_223000000"}}},
    {"kernelName", "minimum_Ascend910_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "e686dcc67183706cd939b6226674441938dba1454e1832cf260fa0dcf59787d9"}
};

static nlohmann::json dynamic_minimum_int8_v80_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "Broadcast"},
    {"_outs_uint1", false},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_base_info", {{"100", {32, 2, 32736, 16368}},
                    {"320", {32, 2, 42320, 21152}},
                    {"230", {32, 2, 42320, 21152}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_attr_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}}}}
};

static nlohmann::json dynamic_minimum_int8_v51 = {
    {"binFileName", "minimum_Ascend310P3_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "minimum_Ascend310P3_int8_210000000"},
                    {"kernelName", "minimum_Ascend310P3_int8_210010000"},
                    {"kernelName", "minimum_Ascend310P3_int8_232000000"},
                    {"kernelName", "minimum_Ascend310P3_int8_223000000"}}},
    {"kernelName", "minimum_Ascend310P3_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "83c8a86aed26418e7d1ad4cf243fdcf6f1b23b72361646db8f16b74af8d89dad"}
};

static nlohmann::json dynamic_minimum_int8_v51_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "Broadcast"},
    {"_outs_uint1", false},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_base_info", {{"100", {8, 2, 32736, 16368}},
                    {"320", {8, 2, 42320, 21152}},
                    {"230", {8, 2, 42320, 21152}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_attr_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"223000000", {}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}}}}
};

static nlohmann::json dynamic_mul_int8_v81_910B1 = {
    {"binFileName", "mul_Ascend910B1_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B1_int8_210000000"},
                    {"kernelName", "mul_Ascend910B1_int8_210010000"},
                    {"kernelName", "mul_Ascend910B1_int8_232000000"},
                    {"kernelName", "mul_Ascend910B1_int8_232010000"},
                    {"kernelName", "mul_Ascend910B1_int8_223000000"},
                    {"kernelName", "mul_Ascend910B1_int8_223010000"}}},
    {"kernelName", "mul_Ascend910B1_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "c31c45b41844b183e95f2ee17ba09f49cd82d9980700b6db051ccd0a757a5248"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_int32_v81_910B1 = {
    {"binFileName", "mul_Ascend910B1_int32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B1_int32_210000000"},
                    {"kernelName", "mul_Ascend910B1_int32_210010000"},
                    {"kernelName", "mul_Ascend910B1_int32_232000000"},
                    {"kernelName", "mul_Ascend910B1_int32_232010000"},
                    {"kernelName", "mul_Ascend910B1_int32_223000000"},
                    {"kernelName", "mul_Ascend910B1_int32_223010000"}}},
    {"kernelName", "mul_Ascend910B1_int32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "59f187725a037b12d5ceeb7f0fc40f4593a47a6c7f35b9c2096ffa02e12e56c8"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_float16_v81_910B1 = {
    {"binFileName", "mul_Ascend910B1_float16"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B1_float16_210000000"},
                    {"kernelName", "mul_Ascend910B1_float16_210010000"},
                    {"kernelName", "mul_Ascend910B1_float16_232000000"},
                    {"kernelName", "mul_Ascend910B1_float16_232010000"},
                    {"kernelName", "mul_Ascend910B1_float16_223000000"},
                    {"kernelName", "mul_Ascend910B1_float16_223010000"}}},
    {"kernelName", "mul_Ascend910B1_float16"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "8736eafb4e0a092bb8a33e9009ffef56312ca80a6249a483d930a7e801ad321f"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_float32_v81_910B1 = {
    {"binFileName", "mul_Ascend910B1_float32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B1_float32_210000000"},
                    {"kernelName", "mul_Ascend910B1_float32_210010000"},
                    {"kernelName", "mul_Ascend910B1_float32_232000000"},
                    {"kernelName", "mul_Ascend910B1_float32_232010000"},
                    {"kernelName", "mul_Ascend910B1_float32_223000000"},
                    {"kernelName", "mul_Ascend910B1_float32_223010000"}}},
    {"kernelName", "mul_Ascend910B1_float32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "68b354fc6b8a269499406d2f2ac4f561af8033760eb1d86b391e1b65ec8a7117"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_int8_v81_910B2 = {
    {"binFileName", "mul_Ascend910B2_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B2_int8_210000000"},
                    {"kernelName", "mul_Ascend910B2_int8_210010000"},
                    {"kernelName", "mul_Ascend910B2_int8_232000000"},
                    {"kernelName", "mul_Ascend910B2_int8_232010000"},
                    {"kernelName", "mul_Ascend910B2_int8_223000000"},
                    {"kernelName", "mul_Ascend910B2_int8_223010000"}}},
    {"kernelName", "mul_Ascend910B2_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "6f6e4aeea1dc18d2e71b6ac1282f66791f6e96e91f0c445a401f453526651e58"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_int32_v81_910B2 = {
    {"binFileName", "mul_Ascend910B2_int32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B2_int32_210000000"},
                    {"kernelName", "mul_Ascend910B2_int32_210010000"},
                    {"kernelName", "mul_Ascend910B2_int32_232000000"},
                    {"kernelName", "mul_Ascend910B2_int32_232010000"},
                    {"kernelName", "mul_Ascend910B2_int32_223000000"},
                    {"kernelName", "mul_Ascend910B2_int32_223010000"}}},
    {"kernelName", "mul_Ascend910B2_int32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "15289ca5379cf295992f53b139580f0a1aff955511bd877229918be5b97b3b89"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_float16_v81_910B2 = {
    {"binFileName", "mul_Ascend910B2_float16"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B2_float16_210000000"},
                    {"kernelName", "mul_Ascend910B2_float16_210010000"},
                    {"kernelName", "mul_Ascend910B2_float16_232000000"},
                    {"kernelName", "mul_Ascend910B2_float16_232010000"},
                    {"kernelName", "mul_Ascend910B2_float16_223000000"},
                    {"kernelName", "mul_Ascend910B2_float16_223010000"}}},
    {"kernelName", "mul_Ascend910B2_float16"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "6529a9272badf649e02965804af55369c94fc583b7ef6697e3c1dc8375dd829a"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_float32_v81_910B2 = {
    {"binFileName", "mul_Ascend910B2_float32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B2_float32_210000000"},
                    {"kernelName", "mul_Ascend910B2_float32_210010000"},
                    {"kernelName", "mul_Ascend910B2_float32_232000000"},
                    {"kernelName", "mul_Ascend910B2_float32_232010000"},
                    {"kernelName", "mul_Ascend910B2_float32_223000000"},
                    {"kernelName", "mul_Ascend910B2_float32_223010000"}}},
    {"kernelName", "mul_Ascend910B2_float32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "b57a1d900efa152db8b78c5b785df249e456f3e5a735384dae936539cacfa06b"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_int8_v81_910B3 = {
    {"binFileName", "mul_Ascend910B3_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B3_int8_210000000"},
                    {"kernelName", "mul_Ascend910B3_int8_210010000"},
                    {"kernelName", "mul_Ascend910B3_int8_232000000"},
                    {"kernelName", "mul_Ascend910B3_int8_232010000"},
                    {"kernelName", "mul_Ascend910B3_int8_223000000"},
                    {"kernelName", "mul_Ascend910B3_int8_223010000"}}},
    {"kernelName", "mul_Ascend910B3_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "ab58b553da40dfd929a1667e38fe4b24202ba75ddb60b4e9513bec5b6830656a"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_int32_v81_910B3 = {
    {"binFileName", "mul_Ascend910B3_int32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B3_int32_210000000"},
                    {"kernelName", "mul_Ascend910B3_int32_210010000"},
                    {"kernelName", "mul_Ascend910B3_int32_232000000"},
                    {"kernelName", "mul_Ascend910B3_int32_232010000"},
                    {"kernelName", "mul_Ascend910B3_int32_223000000"},
                    {"kernelName", "mul_Ascend910B3_int32_223010000"}}},
    {"kernelName", "mul_Ascend910B3_int32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "f4ad7c0a3160e4dfea1295781fcd3182fe29068c95fd139d49b2d08e9b4fd677"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_float16_v81_910B3 = {
    {"binFileName", "mul_Ascend910B3_float16"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B3_float16_210000000"},
                    {"kernelName", "mul_Ascend910B3_float16_210010000"},
                    {"kernelName", "mul_Ascend910B3_float16_232000000"},
                    {"kernelName", "mul_Ascend910B3_float16_232010000"},
                    {"kernelName", "mul_Ascend910B3_float16_223000000"},
                    {"kernelName", "mul_Ascend910B3_float16_223010000"}}},
    {"kernelName", "mul_Ascend910B3_float16"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "8ca3fe4b0d9327ff9bd97bd99ed6ca85ff7ba4f488b68aa7398118a91804f595"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_float32_v81_910B3 = {
    {"binFileName", "mul_Ascend910B3_float32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B3_float32_210000000"},
                    {"kernelName", "mul_Ascend910B3_float32_210010000"},
                    {"kernelName", "mul_Ascend910B3_float32_232000000"},
                    {"kernelName", "mul_Ascend910B3_float32_232010000"},
                    {"kernelName", "mul_Ascend910B3_float32_223000000"},
                    {"kernelName", "mul_Ascend910B3_float32_223010000"}}},
    {"kernelName", "mul_Ascend910B3_float32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "f4a8c43b7d8bb4c59c2d8dd2ba3c158fa946c9cc34e89c56841189e679a7b9a4"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_int8_v81_910B4 = {
    {"binFileName", "mul_Ascend910B4_int8"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B4_int8_210000000"},
                    {"kernelName", "mul_Ascend910B4_int8_210010000"},
                    {"kernelName", "mul_Ascend910B4_int8_232000000"},
                    {"kernelName", "mul_Ascend910B4_int8_232010000"},
                    {"kernelName", "mul_Ascend910B4_int8_223000000"},
                    {"kernelName", "mul_Ascend910B4_int8_223010000"}}},
    {"kernelName", "mul_Ascend910B4_int8"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "c4df8c194fee070a3c422ce976c5149a4f3cfff8837dadc53fe121ed1c03a897"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_int32_v81_910B4 = {
    {"binFileName", "mul_Ascend910B4_int32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B4_int32_210000000"},
                    {"kernelName", "mul_Ascend910B4_int32_210010000"},
                    {"kernelName", "mul_Ascend910B4_int32_232000000"},
                    {"kernelName", "mul_Ascend910B4_int32_232010000"},
                    {"kernelName", "mul_Ascend910B4_int32_223000000"},
                    {"kernelName", "mul_Ascend910B4_int32_223010000"}}},
    {"kernelName", "mul_Ascend910B4_int32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "f70f104fb5b1e4696ffb35202415eab2682e6eeb33ba046efa042721a4c8dc5b"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_float16_v81_910B4 = {
    {"binFileName", "mul_Ascend910B4_float16"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B4_float16_210000000"},
                    {"kernelName", "mul_Ascend910B4_float16_210010000"},
                    {"kernelName", "mul_Ascend910B4_float16_232000000"},
                    {"kernelName", "mul_Ascend910B4_float16_232010000"},
                    {"kernelName", "mul_Ascend910B4_float16_223000000"},
                    {"kernelName", "mul_Ascend910B4_float16_223010000"}}},
    {"kernelName", "mul_Ascend910B4_float16"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "08e202818f20be69f02665497ccb434b10e04ff32739d73c601087b3eabad06e"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_float32_v81_910B4 = {
    {"binFileName", "mul_Ascend910B4_float32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"kernelList", {{"kernelName", "mul_Ascend910B4_float32_210000000"},
                    {"kernelName", "mul_Ascend910B4_float32_210010000"},
                    {"kernelName", "mul_Ascend910B4_float32_232000000"},
                    {"kernelName", "mul_Ascend910B4_float32_232010000"},
                    {"kernelName", "mul_Ascend910B4_float32_223000000"},
                    {"kernelName", "mul_Ascend910B4_float32_223010000"}}},
    {"kernelName", "mul_Ascend910B4_float32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"modeInArgsFirstField", 0},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "ea76e82706ce67ad819a6a361d7a9c922ef749f5ac18e4be4d005c64d79939da"},
    {"taskRation", 1}
};

static nlohmann::json dynamic_mul_int8_v81_910B1_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {25, 4, 12248, 6120}},
                    {"320", {25, 4, 12240, 6120}},
                    {"230", {25, 4, 12240, 6120}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_int32_v81_910B1_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {25, 4, 12288, 6144}},
                    {"320", {25, 4, 12280, 6136}},
                    {"230", {25, 4, 12280, 6136}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_float16_v81_910B1_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {25, 2, 24576, 12288}},
                    {"320", {25, 2, 24560, 12272}},
                    {"230", {25, 2, 24560, 12272}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_float32_v81_910B1_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {25, 4, 12288, 6144}},
                    {"320", {25, 4, 12280, 6136}},
                    {"230", {25, 4, 12280, 6136}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_int8_v81_910B2_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {24, 4, 12248, 6120}},
                    {"320", {24, 4, 12240, 6120}},
                    {"230", {24, 4, 12240, 6120}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_int32_v81_910B2_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {24, 4, 12288, 6144}},
                    {"320", {24, 4, 12280, 6136}},
                    {"230", {24, 4, 12280, 6136}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_float16_v81_910B2_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {24, 2, 24576, 12288}},
                    {"320", {24, 2, 24560, 12272}},
                    {"230", {24, 2, 24560, 12272}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_float32_v81_910B2_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {24, 4, 12288, 6144}},
                    {"320", {24, 4, 12280, 6136}},
                    {"230", {24, 4, 12280, 6136}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_int8_v81_910B3_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {20, 4, 12248, 6120}},
                    {"320", {20, 4, 12240, 6120}},
                    {"230", {20, 4, 12240, 6120}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_int32_v81_910B3_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {20, 4, 12288, 6144}},
                    {"320", {20, 4, 12280, 6136}},
                    {"230", {20, 4, 12280, 6136}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_float16_v81_910B3_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {20, 2, 24576, 12288}},
                    {"320", {20, 2, 24560, 12272}},
                    {"230", {20, 2, 24560, 12272}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_float32_v81_910B3_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {20, 4, 12288, 6144}},
                    {"320", {20, 4, 12280, 6136}},
                    {"230", {20, 4, 12280, 6136}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_int8_v81_910B4_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {20, 4, 12248, 6120}},
                    {"320", {20, 4, 12240, 6120}},
                    {"230", {20, 4, 12240, 6120}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_int32_v81_910B4_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {20, 4, 12288, 6144}},
                    {"320", {20, 4, 12280, 6136}},
                    {"230", {20, 4, 12280, 6136}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_float16_v81_910B4_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {20, 2, 24576, 12288}},
                    {"320", {20, 2, 24560, 12272}},
                    {"230", {20, 2, 24560, 12272}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json dynamic_mul_float32_v81_910B4_tiling_info = {
    {"_fusion_index", {{0, 1}}},
    {"_pattern", "ElemWise"},
    {"_ub_factor_align", 128},
    {"_flag_info", {false, false, true, true, true, false}},
    {"_classify_inputs_num", 2},
    {"_base_info", {{"100", {20, 4, 12288, 6144}},
                    {"320", {20, 4, 12280, 6136}},
                    {"230", {20, 4, 12280, 6136}}}},
    {"_elewise_vars", {{"210000000", {10000, 20000, 30000}},
                       {"210010000", {10000, 20000, 30000}},
                       {"232000000", {10001, 20000, 30000}},
                       {"232010000", {10001, 20000, 30000}},
                       {"223000000", {10000, 20000, 30000}},
                       {"223010000", {10000, 20000, 30000}}}},
    {"_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
               {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
               {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_normal_vars", {{"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"232000000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"232010000", {"_dim_0_1", "_block_factor_0", "_ub_factor_0"}},
                      {"223000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
                      {"223010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}}}},
    {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"232000000", {}}, {"232010000", {}}, {"223000000", {}},
        {"223010000", {}}}}
};

static nlohmann::json MemSet_dynamic_AtomicAddrClean_1_ascend910 = {
    {"binFileName", "MemSet_dynamic_AtomicAddrClean_1_ascend910"},
    {"binFileSuffix", ".o"},
    {"blockDim", 32},
    {"deterministic", "ignore"},
    {"kernelName", "MemSet_dynamic_AtomicAddrClean_1_ascend910__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {}},
    {"opParaSize", 8589934588},
    {"parameters", {0, 0, 0}},
    {"sha256", "9df29211093c719575d552c8bfd12cdb1e8199a77ea3a9c8b75fa451a1c554e9"},
    {"wspMode", true}
};

static nlohmann::json MemSet_dynamic_AtomicAddrClean_1_ascend910_tiling_info = {
    {"vars", {{"ub_size", 262144},
              {"core_num", 32},
              {"workspace_num", -1},
              {"max_repeat_time", -1},
              {"mask_nums", {64}},
              {"byte_list", {4}},
              {"_workspace_index_list", {}},
              {"is_dynamic", true}}}
};

static nlohmann::json MemSet_dynamic_AtomicAddrClean_1_ascend310p3 = {
    {"binFileName", "MemSet_dynamic_AtomicAddrClean_1_ascend310p3"},
    {"binFileSuffix", ".o"},
    {"blockDim", 8},
    {"deterministic", "ignore"},
    {"kernelName", "MemSet_dynamic_AtomicAddrClean_1_ascend310p3__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {}},
    {"opParaSize", 8589934588},
    {"parameters", {0, 0, 0}},
    {"sha256", "8a6c24659ba2ed1b77cd60c79c6b213531e82d7b359cb9845a226467bb98da08"},
    {"wspMode", true}
};

static nlohmann::json MemSet_dynamic_AtomicAddrClean_1_ascend310p3_tiling_info = {
    {"vars", {{"ub_size", 262144},
              {"core_num", 8},
              {"workspace_num", -1},
              {"max_repeat_time", -1},
              {"mask_nums", {64}},
              {"byte_list", {4}},
              {"_workspace_index_list", {}},
              {"is_dynamic", true}}}
};

static nlohmann::json MemSet_dynamic_AtomicAddrClean_1_ascend910b = {
    {"binFileName", "MemSet_dynamic_AtomicAddrClean_1_ascend910b"},
    {"binFileSuffix", ".o"},
    {"blockDim", 48},
    {"core_type", "AIV"},
    {"deterministic", "ignore"},
    {"kernelName", "MemSet_dynamic_AtomicAddrClean_1_ascend910b__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"memoryStamping", {}},
    {"modeInArgsFirstField", {0}},
    {"opParaSize", 8589934588},
    {"parameters", {0, 0, 0}},
    {"sha256", "c2fd2c928c592f5a5630eca132aa61bda75db1d65a2a8059c72f5bb82623bf81"},
    {"taskRation", {0}},
    {"wspMode", true}
};

static nlohmann::json MemSet_dynamic_AtomicAddrClean_1_ascend910b_tiling_info = {
    {"vars", {{"ub_size", 262144},
              {"core_num", 48},
              {"workspace_num", -1},
              {"max_repeat_time", -1},
              {"mask_nums", {64}},
              {"byte_list", {4}},
              {"_workspace_index_list", {}},
              {"is_dynamic", true}}}
};

// 910A
static nlohmann::json te_gatherv2_08c676dd0564eaf646f0054fc0fb4adc559d7dca0155d698d2b788ce8a3a6a39_1 = {
    {"binFileName", "te_gatherv2_08c676dd0564eaf646f0054fc0fb4adc559d7dca0155d698d2b788ce8a3a6a39_1"},
    {"binFileSuffix", ".o"},
    {"blockDim", 32},
    {"core_type", "AiCore"},
    {"deterministic", "ignore"},
    {"kernelName", "te_gatherv2_08c676dd0564eaf646f0054fc0fb4adc559d7dca0155d698d2b788ce8a3a6a39_1__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {-1}},
    {"opParaSize", 0},
    {"parameters", {0, 0, 0, 0, 0}},
    {"sha256", "85823e2914ed992ad57a5d327eb50e5e1151dd6816054d9c5e505025b6acfa75"},
    {"globalworkspace_spec_workspace", {{"size", 32},
                                        {"type", 0}}}
};

static nlohmann::json te_gatherv2_08c676dd0564eaf646f0054fc0fb4adc559d7dca0155d698d2b788ce8a3a6a39_1_tiling_info = {
    {"compileInfo", {{"_sgt_cube_vector_core_type", "AiCore"},
                     {"device_id", 0},
                     {"is_gather_v2", true},
                     {"is_tik", true},
                     {"vars", {{"core_num", 32},
                               {"impl_mode", 0},
                               {"indices_dsize", 8},
                               {"is_preprocessed", false},
                               {"l1_size", 1048576},
                               {"params_dsize", 4},
                               {"ub_size", 262144}}}}}
};

// 910A high performance
static nlohmann::json te_gatherv2_187b0f20dcf51283c20135753f6879d71370acd742114d5cb4847c86e660cd3c_1 = {
    {"binFileName", "te_gatherv2_187b0f20dcf51283c20135753f6879d71370acd742114d5cb4847c86e660cd3c_1"},
    {"binFileSuffix", ".o"},
    {"blockDim", 32},
    {"core_type", "AiCore"},
    {"deterministic", "ignore"},
    {"kernelName", "te_gatherv2_187b0f20dcf51283c20135753f6879d71370acd742114d5cb4847c86e660cd3c_1__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {-1}},
    {"opParaSize", 256},
    {"parameters", {0, 0, 0, 0, 0}},
    {"sha256", "d7ba182520a088e03789598fed5863deb0a6afbc64af418d266e421d7cca1491"},
    {"globalworkspace_spec_workspace", {{"size", 32},
                                        {"type", 0}}}
};

static nlohmann::json te_gatherv2_187b0f20dcf51283c20135753f6879d71370acd742114d5cb4847c86e660cd3c_1_tiling_info = {
    {"compileInfo", {{"_sgt_cube_vector_core_type", "AiCore"},
                     {"device_id", 0},
                     {"is_gather_v2", true},
                     {"is_tik", true},
                     {"vars", {{"core_num", 32},
                               {"impl_mode", 1},
                               {"indices_dsize", 4},
                               {"is_preprocessed", false},
                               {"l1_size", 1048576},
                               {"params_dsize", 4},
                               {"ub_size", 262144}}}}}
};

// 910B1B2 int32
static nlohmann::json gatherv2_Ascend910B_float32_B1B2 = {
    {"binFileName", "gatherv2_Ascend910B_float32_B1B2"},
    {"binFileSuffix", ".o"},
    {"blockDim", 48},
    {"core_type", "VectorCore"},
    {"deterministic", "ignore"},
    {"intercoreSync", 0},
    {"is_gather_v2", true},
    {"is_tik", true},
    {"vars", {{"core_num", 48},
            {"impl_mode", 1},
            {"indices_dsize", 4},
            {"is_preprocessed", false},
            {"l1_size", 1048576},
            {"params_dsize", 4},
            {"ub_size", 196608}}},
    {"kernelName", "gatherv2_Ascend910B_float32_B1B2"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"memoryStamping", {-1}},
    {"opParaSize", 256},
    {"parameters", {0, 0, 0, 0, 0}},
    {"sha256", "13af59b4bc82c95d08df5bca2b6d7fbc3946fbb4027c86fd4b1c49da13360b28"}
};

// 910B3B4 int32
static nlohmann::json gatherv2_Ascend910B_float32_B3B4 = {
    {"binFileName", "gatherv2_Ascend910B_float32_B3B4"},
    {"binFileSuffix", ".o"},
    {"blockDim", 40},
    {"core_type", "VectorCore"},
    {"deterministic", "ignore"},
    {"intercoreSync", 0},
    {"is_gather_v2", true},
    {"is_tik", true},
    {"vars", {{"core_num", 40},
            {"impl_mode", 1},
            {"indices_dsize", 4},
            {"is_preprocessed", false},
            {"l1_size", 1048576},
            {"params_dsize", 4},
            {"ub_size", 196608}}},
    {"kernelName", "gatherv2_Ascend910B_float32_B3B4"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"memoryStamping", {-1}},
    {"opParaSize", 256},
    {"parameters", {0, 0, 0, 0, 0}},
    {"sha256", "13af59b4bc82c95d08df5bca2b6d7fbc3946fbb4027c86fd4b1c49da13360b28"}
};

// 310P
static nlohmann::json te_gatherv2_ca87d1f41c037f24ece7993bc5c6f9afc00be851bcbaef7c03daf96701a30810_1 = {
    {"binFileName", "te_gatherv2_ca87d1f41c037f24ece7993bc5c6f9afc00be851bcbaef7c03daf96701a30810_1"},
    {"binFileSuffix", ".o"},
    {"blockDim", 8},
    {"core_type", "AiCore"},
    {"deterministic", "ignore"},
    {"kernelName", "te_gatherv2_ca87d1f41c037f24ece7993bc5c6f9afc00be851bcbaef7c03daf96701a30810_1__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {-1}},
    {"opParaSize", 256},
    {"parameters", {0, 0, 0, 0, 0}},
    {"sha256", "a3ce04fdeb3b3ac5df74aff7bb5ce9b01037fa93251cbe9d2a33489db4c89927"},
    {"globalworkspace_spec_workspace", {{"size", 32},
                                        {"type", 0}}}
};

static nlohmann::json te_gatherv2_ca87d1f41c037f24ece7993bc5c6f9afc00be851bcbaef7c03daf96701a30810_1_tiling_info = {
    {"compileInfo", {{"_sgt_cube_vector_core_type", "AiCore"},
                     {"device_id", 0},
                     {"is_gather_v2", true},
                     {"is_tik", true},
                     {"vars", {{"core_num", 8},
                               {"impl_mode", 0},
                               {"indices_dsize", 8},
                               {"is_preprocessed", false},
                               {"l1_size", 1048576},
                               {"params_dsize", 4},
                               {"ub_size", 262144}}}}}
};

// 310P high performance
static nlohmann::json te_gatherv2_ec8c036da6e525f834dabb7a436b50387bbc5929f5201b28f0db7bd63fdb3a9b_1 = {
    {"binFileName", "te_gatherv2_ec8c036da6e525f834dabb7a436b50387bbc5929f5201b28f0db7bd63fdb3a9b_1"},
    {"binFileSuffix", ".o"},
    {"blockDim", 8},
    {"core_type", "AiCore"},
    {"deterministic", "ignore"},
    {"kernelName", "te_gatherv2_ec8c036da6e525f834dabb7a436b50387bbc5929f5201b28f0db7bd63fdb3a9b_1__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {-1}},
    {"opParaSize", 256},
    {"parameters", {0, 0, 0, 0, 0}},
    {"sha256", "bcacf0448a201b92e0a4a279d0670695539811753ea2494ef707918abad3b22c"},
    {"globalworkspace_spec_workspace", {{"size", 32},
                                        {"type", 0}}}
};

static nlohmann::json te_gatherv2_ec8c036da6e525f834dabb7a436b50387bbc5929f5201b28f0db7bd63fdb3a9b_1_tiling_info = {
    {"compileInfo", {{"_sgt_cube_vector_core_type", "AiCore"},
                     {"device_id", 0},
                     {"is_gather_v2", true},
                     {"is_tik", true},
                     {"vars", {{"core_num", 8},
                               {"impl_mode", 0},
                               {"indices_dsize", 4},
                               {"is_preprocessed", false},
                               {"l1_size", 1048576},
                               {"params_dsize", 4},
                               {"ub_size", 262144}}}}}
};

// 910A
static nlohmann::json te_unsortedsegmentsum_72b2c8c4c1584aa29ce28fc90a2f137f8e986479aa7adc062001be857271aea5_1 = {
    {"binFileName", "te_unsortedsegmentsum_72b2c8c4c1584aa29ce28fc90a2f137f8e986479aa7adc062001be857271aea5_1"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"core_type", "AiCore"},
    {"deterministic", "false"},
    {"kernelName", "te_unsortedsegmentsum_72b2c8c4c1584aa29ce28fc90a2f137f8e986479aa7adc062001be857271aea5_1__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {-1}},
    {"opParaSize", 512},
    {"parameters", {0,
                    0,
                    0,
                    {{"dtype", "float32"},
                     {"init_value", 0.0}},
                    0}},
    {"sha256", "02be4ce616557a7e5d26f6f4c4118f88f1bb802e1faa04ea8a16c5c13cf46588"},
    {"globalworkspace_spec_workspace", {{"size", 32},
                                        {"type", 0}}}
};

static nlohmann::json
    te_unsortedsegmentsum_72b2c8c4c1584aa29ce28fc90a2f137f8e986479aa7adc062001be857271aea5_1_tiling_info = {
    {"compileInfo", {{"_sgt_cube_vector_core_type", "AiCore"},
                     {"device_id", 0},
                     {"is_tik", true},
                     {"vars", {{"core_num", 32},
                               {"impl_mode", 0},
                               {"dtype", "float32"},
                               {"ub_size", 259584},
                               {"ub_tensor_num", 3}}}}}
};

// 910A high performance
static nlohmann::json te_unsortedsegmentsum_da0a5264250330a6c92ab5b622124e7d75359168a46ae6eab47256ad57367ba9_1 = {
    {"binFileName", "te_unsortedsegmentsum_da0a5264250330a6c92ab5b622124e7d75359168a46ae6eab47256ad57367ba9_1"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"core_type", "AiCore"},
    {"deterministic", "false"},
    {"kernelName", "te_unsortedsegmentsum_da0a5264250330a6c92ab5b622124e7d75359168a46ae6eab47256ad57367ba9_1__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {-1}},
    {"opParaSize", 512},
    {"parameters", {0,
                    0,
                    0,
                    {{"dtype", "float32"},
                     {"init_value", 0.0}},
                    0}},
    {"sha256", "3ca6f6a930fa634541d3f99b115ba0671d81e4605ed1dc41d512bbfd10a4fb4f"},
    {"globalworkspace_spec_workspace", {{"size", 32},
                                        {"type", 0}}}
};

static nlohmann::json
    te_unsortedsegmentsum_da0a5264250330a6c92ab5b622124e7d75359168a46ae6eab47256ad57367ba9_1_tiling_info = {
    {"compileInfo", {{"_sgt_cube_vector_core_type", "AiCore"},
                     {"device_id", 0},
                     {"is_tik", true},
                     {"vars", {{"core_num", 32},
                               {"impl_mode", 1},
                               {"dtype", "float32"},
                               {"ub_size", 259584},
                               {"ub_tensor_num", 3}}}}}
};

// 910B1~B4 32int
static nlohmann::json uss_Ascend910B4_float32 = {
    {"binFileName", "uss_Ascend910B4_float32"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"core_type", "VectorCore"},
    {"deterministic", "false"},
    {"intercoreSync", 0},
    {"_pattern", "Segment"},
    {"device_id", "None"},
    {"_cube_vector_core_type", "VectorCore"},
    {"_base_info", {48, 196576, 0, 4, 4}},
    {"_custom_info", {1, 1, 0, false}},
    {"_tensor_sizes", {
        {"81", {3848, 962, 4096}},
        {"71", {13104, 3276, 4096}},
        {"61", {8184, 8184, 8184}},
        {"21", {8184, 8184, 8184}},
        {"11", {16376, 16376, 16376}},
        {"20", {9824, 9824, 9824}},
        {"10", {24568, 24568, 24568}}}},
    {"_segment_vars", {
        {"770018110", {10000, 10001, 20000, 30000, 60000, 50000}},
        {"770017110", {10000, 10001, 20000, 30000, 60000, 50000}},
        {"770008110", {10000, 10001, 20000, 30000, 60000, 50000}},
        {"770007110", {10000, 10001, 20000, 30000, 60000, 50000}},
        {"770016100", {10000, 10001, 20000, 30000, 60000, 50000}},
        {"770002000", {10000, 10001, 20000, 30000, 50000}},
        {"770001000", {10000, 10001, 20000, 30000, 50000}},
        {"770001001", {10000, 10001, 20000, 30000, 40001, 50000}},
        {"770001100", {10000, 10001, 20000, 30000, 60000, 50000}},
        {"770002100", {10000, 10001, 20000, 30000, 60000, 50000}},
        {"770011100", {10000, 10001, 20000, 30000, 60000, 50000}},
        {"770012100", {10000, 10001, 20000, 30000, 60000, 50000}}}},
    {"_int64_mode", false},
    {"vars", {
        {"core_num", 32},
        {"impl_mode", 1},
        {"dtype", "float32"},
        {"ub_size", 259584},
        {"ub_tensor_num", 3}}},
    {"kernelList", {
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770001000"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770001001"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770002000"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770007110"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770008110"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770016100"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770017110"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770018110"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770001100"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770002100"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770011100"}
        },
        {
            {"deterministic", "false"},
            {"kernelName", "uss_Ascend910B4_float32_770012100"}
        }}},
    {"kernelName", "uss_Ascend910B4_float32"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"memoryStamping", {0}},
    {"opParaSize", 48},
    {"parameters", {
        0,
        0,
        0,
        {
            {"dtype", "float32"},
            {"init_value", 0.0}},
        0}},
    {"sha256", "5c39aa45d926501c6b5821efbfad9af0de0d3ad4709775fb87aec7a474160cd3"}
};

// 310P
static nlohmann::json te_unsortedsegmentsum_d0a8c7e68142042963e855ce6004fa68d52228200ba7cf5426e5ab47e86ae38c_1 = {
    {"binFileName", "te_unsortedsegmentsum_d0a8c7e68142042963e855ce6004fa68d52228200ba7cf5426e5ab47e86ae38c_1"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"core_type", "AiCore"},
    {"deterministic", "false"},
    {"kernelName", "te_unsortedsegmentsum_d0a8c7e68142042963e855ce6004fa68d52228200ba7cf5426e5ab47e86ae38c_1__kernel0"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {-1}},
    {"opParaSize", 512},
    {"parameters", {0,
                    0,
                    0,
                    {{"dtype", "float32"},
                     {"init_value", 0.0}},
                    0}},
    {"sha256", "5d5e38e1bd52ef58eb6f99457127e7e1249efed7922d4bd2fbc19489841939a6"},
    {"globalworkspace_spec_workspace", {{"size", 32},
                                        {"type", 0}}}
};

static nlohmann::json
    te_unsortedsegmentsum_d0a8c7e68142042963e855ce6004fa68d52228200ba7cf5426e5ab47e86ae38c_1_tiling_info = {
    {"compileInfo", {{"_sgt_cube_vector_core_type", "AiCore"},
                     {"device_id", 0},
                     {"is_tik", true},
                     {"vars", {{"core_num", 8},
                               {"impl_mode", 0},
                               {"dtype", "float32"},
                               {"ub_size", 259584},
                               {"ub_tensor_num", 3}}}}}
};

static nlohmann::json dynamic_add_int64_v80 = {
    {"binFileName", "add_Ascend910_int64"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"compileInfo", {
        {"_base_info", {{"100", {32, 8, 5436, 2716}}}},
        {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"2147483647", {}}}},
        {"_elewise_disable_fuse", false},
        {"_elewise_vars", {
			{"210000000", {10000, 20000, 30000}},
            {"210010000", {10000, 20000, 30000}}}},
        {"_enable_fractal_format", false},
        {"_inputs_ele_in_block", {4, 4}},
        {"_int64_mode", false},
        {"_is_const_shapes", false},
        {"_is_pure_elewise", true},
        {"_max_out_dtype_num", 4},
        {"_normal_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}}},
        {"_pattern", "ElemWise"},
        {"_ub_block_size", 32},
        {"_ub_factor_align", 128},
        {"_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}}}}},
    {"coreType", "AiCore"},
    {"deterministic", "ignore"},
    {"globalworkspace_spec_workspace", {{"size", 32}, {"type", 0}}},
    {"kernelList", {
        {
            {"deterministic", "ignore"},
            {"kernelName", "add_Ascend910_int64_2147483647"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "add_Ascend910_int64_210000000"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "add_Ascend910_int64_210010000"}
        }
    }},
    {"kernelName", "add_Ascend910_int64"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {}},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "13635c20edb52a50af659683576ff94c799638549294bee7014c543a970e1109"},
    {"supportInfo", {
        {"int64Mode", false},
        {"staticKey", "404d0b7a58ef287687a3d011e31ac4a11b1dabab81260be2655418ec1f2a7430"},
        {"inputs", {
            {{"name", "x1"}, {"index", 0}, {"dtype", "int64"}, {"format", "ND"}, {"paramType", "required"},
                {"shape", {-2}}, {"format_match_mode", "FormatAgnostic"}},
            {{"name", "x2"}, {"index", 1}, {"dtype", "int64"}, {"format", "ND"}, {"paramType", "required"},
                {"shape", {-2}}, {"format_match_mode", "FormatAgnostic"}}}},
        {"outputs", {
            {{"name", "y"}, {"index", 0}, {"dtype", "int64"}, {"format", "ND"}, {"paramType", "required"},
                {"shape", {-2}}, {"format_match_mode", "FormatAgnostic"}}}},
        {"deterministic", "ignore"}
    }}
};

static nlohmann::json dynamic_mul_int64_v80 = {
    {"binFileName", "mul_Ascend910_int64"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"compileInfo", {
        {"_base_info", {{"100", {32, 8, 8192, 4096}}}},
        {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"2147483647", {}}}},
        {"_elewise_disable_fuse", false},
        {"_elewise_vars", {
            {"210000000", {10000, 20000, 30000}},
            {"210010000", {10000, 20000, 30000}}}},
        {"_enable_fractal_format", false},
        {"_inputs_ele_in_block", {4, 4}},
        {"_int64_mode", false},
        {"_is_const_shapes", false},
        {"_is_pure_elewise", true},
        {"_max_out_dtype_num", 4},
        {"_normal_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }},
        {"_pattern", "ElemWise"},
        {"_ub_block_size", 32},
        {"_ub_factor_align", 128},
        {"_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }}
    }},
    {"coreType", "AiCore"},
    {"deterministic", "ignore"},
    {"globalworkspace_spec_workspace", {{"size", 32}, {"type", 0}}},
    {"kernelList", {
        {
            {"deterministic", "ignore"},
            {"kernelName", "mul_Ascend910_int64_2147483647"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "mul_Ascend910_int64_210000000"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "mul_Ascend910_int64_210010000"}
        }
    }},
    {"kernelName", "mul_Ascend910_int64"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {}},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "d5565c17cf19729742b43132369953b061504ac08643b659dc3a2a3b5fab5db4"},
    {"supportInfo", {
        {"int64Mode", false},
        {"staticKey", "404d0b7a58ef287687a3d011e31ac4a11b1dabab81260be2655418ec1f2a7430"},
        {"inputs", {
            {
                {"name", "x1"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            },
            {
                {"name", "x2"},
                {"index", 1},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }
    }},
        {"outputs", {
            {
                {"name", "y"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }
        }},
        {"deterministic", "ignore"}
    }}
};

static nlohmann::json dynamic_maximum_int64_v80 = {
    {"binFileName", "max_Ascend910_int64"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"compileInfo", {
        {"_base_info", {{"100", {32, 8, 8188, 4092}}}},
        {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"2147483647", {}}}},
        {"_elewise_disable_fuse", false},
        {"_elewise_vars", {
            {"210000000", {10000, 20000, 30000}},
            {"210010000", {10000, 20000, 30000}}}},
        {"_enable_fractal_format", false},
        {"_inputs_ele_in_block", {4, 4}},
        {"_int64_mode", false},
        {"_is_const_shapes", false},
        {"_is_pure_elewise", true},
        {"_max_out_dtype_num", 4},
        {"_normal_vars", {
            {"210000000", {
                "_dim_0_0",
                "_block_factor_0",
                "_ub_factor_0"
            }},
            {"210010000", {
                "_dim_0_0",
                "_block_factor_0",
                "_ub_factor_0"
            }},
            {"2147483647", {}}
        }},
        {"_pattern", "ElemWise"},
        {"_ub_block_size", 32},
        {"_ub_factor_align", 128},
        {"_vars", {
            {"210000000", {
                "_dim_0_0",
                "_block_factor_0",
                "_ub_factor_0"
            }},
            {"210010000", {
                "_dim_0_0",
                "_block_factor_0",
                "_ub_factor_0"
            }},
            {"2147483647", {}}
        }}
    }},
    {"coreType", "AiCore"},
    {"deterministic", "ignore"},
    {"globalworkspace_spec_workspace", {{"size", 32}, {"type", 0}}},
    {"kernelList", {
        {
            {"deterministic", "ignore"},
            {"kernelName", "max_Ascend910_int64_2147483647"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "max_Ascend910_int64_210000000"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "max_Ascend910_int64_210010000"}
        }
    }},
    {"kernelName", "max_Ascend910_int64"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {}},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "32ca21d563c9145294f7e4b51300c0c933999b1a748afc7bd377c004f6523ea6"},
    {"supportInfo", {
        {"int64Mode", false},
        {"staticKey", "404d0b7a58ef287687a3d011e31ac4a11b1dabab81260be2655418ec1f2a7430"},
        {"inputs", {
            {
                {"name", "x1"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            },
            {
                {"name", "x2"},
                {"index", 1},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }
        }},
        {"outputs", {
            {
                {"name", "y"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }
        }},
        {"deterministic", "ignore"}
    }}
};

static nlohmann::json dynamic_minimum_int64_v80 = {
    {"binFileName", "min_Ascend910_int64"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"compileInfo", {{"_base_info", {{"100", {32, 8, 8188, 4092}}}},
        {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"2147483647", {}}}},
        {"_elewise_disable_fuse", false},
        {"_elewise_vars", {
            {"210000000", {10000, 20000, 30000}},
            {"210010000", {10000, 20000, 30000}}}},
        {"_enable_fractal_format", false},
        {"_inputs_ele_in_block", {4, 4}},
        {"_int64_mode", false},
        {"_is_const_shapes", false},
        {"_is_pure_elewise", true},
        {"_max_out_dtype_num", 4},
        {"_normal_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }},
        {"_pattern", "ElemWise"},
        {"_ub_block_size", 32},
        {"_ub_factor_align", 128},
        {"_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }}
    }},
    {"coreType", "AiCore"},
    {"deterministic", "ignore"},
    {"globalworkspace_spec_workspace", {{"size", 32}, {"type", 0}}},
    {"kernelList", {
        {
            {"deterministic", "ignore"},
            {"kernelName", "min_Ascend910_int64_2147483647"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "min_Ascend910_int64_210000000"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "min_Ascend910_int64_210010000"}
        }
    }},
    {"kernelName", "min_Ascend910_int64"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF"},
    {"memoryStamping", {}},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "2e68df838bd94060ff70915fb96a93b5d8727db8d53dfd091a75c6f9878c8109"},
    {"supportInfo", {
        {"int64Mode", false},
        {"staticKey", "404d0b7a58ef287687a3d011e31ac4a11b1dabab81260be2655418ec1f2a7430"},
        {"inputs", {
            {
                {"name", "x1"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            },
            {
                {"name", "x2"},
                {"index", 1},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }}},
        {"outputs", {
            {
                {"name", "y"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }
        }},
        {"deterministic", "ignore"}
    }}
};

static nlohmann::json dynamic_add_int64_v81 = {
    {"binFileName", "add_Ascend910b_int64"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"compileInfo", {
        {"_base_info", {{"100", {40, 8, 3492, 1744}}}},
        {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"2147483647", {}}}},
        {"_elewise_disable_fuse", false},
        {"_elewise_vars", {
            {"210000000", {10000, 20000, 30000}},
            {"210010000", {10000, 20000, 30000}}
        }},
        {"_enable_fractal_format", false},
        {"_inputs_ele_in_block", {4, 4}},
        {"_int64_mode", false},
        {"_is_const_shapes", false},
        {"_is_pure_elewise", true},
        {"_max_out_dtype_num", 4},
        {"_normal_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }},
        {"_pattern", "ElemWise"},
        {"_ub_block_size", 32},
        {"_ub_factor_align", 128},
        {"_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }}
    }},
    {"coreType", "VectorCore"},
    {"deterministic", "ignore"},
    {"intercoreSync", 0},
    {"kernelList", {
        {
            {"deterministic", "ignore"},
            {"kernelName", "add_Ascend910b_int64_2147483647"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "add_Ascend910b_int64_210000000"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "add_Ascend910b_int64_210010000"}
        }
    }},
    {"kernelName", "add_Ascend910b_int64"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"memoryStamping", {}},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "2f9c1806acfd05d9ebc5b5cbf4ea6c6d8707c9e9cd104433416f48c55b9da560"},
    {"supportInfo", {
        {"int64Mode", false},
        {"staticKey", "404d0b7a58ef287687a3d011e31ac4a11b1dabab81260be2655418ec1f2a7430"},
        {"inputs", {
            {
                {"name", "x1"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            },
            {
                {"name", "x2"},
                {"index", 1},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }}},
        {"outputs", {
            {
                {"name", "y"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }
        }},
        {"deterministic", "ignore"}
    }}
};

static nlohmann::json dynamic_mul_int64_v81 = {
    {"binFileName", "mul_Ascend910b_int64"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"compileInfo", {
        {"_base_info", {{"100", {40, 8, 6144, 3072}}}},
        {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"2147483647", {}}}},
        {"_elewise_disable_fuse", false},
        {"_elewise_vars", {
            {"210000000", {10000, 20000, 30000}},
            {"210010000", {10000, 20000, 30000}}}},
        {"_enable_fractal_format", false},
        {"_inputs_ele_in_block", {4, 4}},
        {"_int64_mode", false},
        {"_is_const_shapes", false},
        {"_is_pure_elewise", true},
        {"_max_out_dtype_num", 4},
        {"_normal_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }},
        {"_pattern", "ElemWise"},
        {"_ub_block_size", 32},
        {"_ub_factor_align", 128},
        {"_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }}
    }},
    {"coreType", "VectorCore"},
    {"deterministic", "ignore"},
    {"intercoreSync", 0},
    {"kernelList", {
        {
            {"deterministic", "ignore"},
            {"kernelName", "mul_Ascend910b_int64_2147483647"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "mul_Ascend910b_int64_210000000"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "mul_Ascend910b_int64_210010000"}
        }
    }},
    {"kernelName", "mul_Ascend910b_int64"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"memoryStamping", {}},
    {"memoryStamping", {}},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "ccaf1346a3358427ec1c4baef30fde47ca45e8f023445715fad529a0cb6c13b5"},
    {"supportInfo", {
        {"int64Mode", false},
        {"staticKey", "404d0b7a58ef287687a3d011e31ac4a11b1dabab81260be2655418ec1f2a7430"},
        {"inputs", {
            {
                {"name", "x1"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            },
            {
                {"name", "x2"},
                {"index", 1},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
        }}},
        {"outputs", {
            {
                {"name", "y"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }
        }},
        {"deterministic", "ignore"}
    }}
};

static nlohmann::json dynamic_maximum_int64_v81 = {
    {"binFileName", "max_Ascend910b_int64"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"compileInfo", {{"_base_info", {{"100", {48, 8, 6140, 3068}}}},
        {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"2147483647", {}}}},
        {"_elewise_disable_fuse", false},
        {"_elewise_vars", {
            {"210000000", {10000, 20000, 30000}},
            {"210010000", {10000, 20000, 30000}}}},
        {"_enable_fractal_format", false},
        {"_inputs_ele_in_block", {4, 4}},
        {"_int64_mode", false},
        {"_is_const_shapes", false},
        {"_is_pure_elewise", true},
        {"_max_out_dtype_num", 4},
        {"_normal_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }},
        {"_pattern", "ElemWise"},
        {"_ub_block_size", 32},
        {"_ub_factor_align", 128},
        {"_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }}
    }},
    {"coreType", "VectorCore"},
    {"deterministic", "ignore"},
    {"intercoreSync", 0},
    {"kernelList", {
        {
            {"deterministic", "ignore"},
            {"kernelName", "max_Ascend910b_int64_2147483647"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "max_Ascend910b_int64_210000000"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "max_Ascend910b_int64_210010000"}
        }
    }},
    {"kernelName", "max_Ascend910b_int64"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"memoryStamping", {}},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "3244da555d3c6f25e113dddf8ff3a047b5a517fcbaf3d8777a5a5d000f6bfbc0"},
    {"supportInfo", {
        {"int64Mode", false},
        {"staticKey", "404d0b7a58ef287687a3d011e31ac4a11b1dabab81260be2655418ec1f2a7430"},
        {"inputs", {
            {
                {"name", "x1"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            },
            {
                {"name", "x2"},
                {"index", 1},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }}},
        {"outputs", {
            {
                {"name", "y"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }
        }},
        {"deterministic", "ignore"}
    }}
};

static nlohmann::json dynamic_minimum_int64_v81 = {
    {"binFileName", "min_Ascend910b_int64"},
    {"binFileSuffix", ".o"},
    {"blockDim", -1},
    {"compileInfo", {{"_base_info", {{"100", {48, 8, 6140, 3068}}}},
        {"_custom_vars", {{"210000000", {}}, {"210010000", {}}, {"2147483647", {}}}},
        {"_elewise_disable_fuse", false},
        {"_elewise_vars", {
            {"210000000", {10000, 20000, 30000}},
            {"210010000", {10000, 20000, 30000}}}},
        {"_enable_fractal_format", false},
        {"_inputs_ele_in_block", {4, 4}},
        {"_int64_mode", false},
        {"_is_const_shapes", false},
        {"_is_pure_elewise", true},
        {"_max_out_dtype_num", 4},
        {"_normal_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }},
        {"_pattern", "ElemWise"},
        {"_ub_block_size", 32},
        {"_ub_factor_align", 128},
        {"_vars", {
            {"210000000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"210010000", {"_dim_0_0", "_block_factor_0", "_ub_factor_0"}},
            {"2147483647", {}}
        }}
    }},
    {"coreType", "VectorCore"},
    {"deterministic", "ignore"},
    {"intercoreSync", 0},
    {"kernelList", {
        {
            {"deterministic", "ignore"},
            {"kernelName", "min_Ascend910b_int64_2147483647"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "min_Ascend910b_int64_210000000"}
        },
        {
            {"deterministic", "ignore"},
            {"kernelName", "min_Ascend910b_int64_210010000"}
        }
    }},
    {"kernelName", "min_Ascend910b_int64"},
    {"magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC"},
    {"memoryStamping", {}},
    {"opParaSize", 12},
    {"parameters", {0, 0, 0, 0}},
    {"sha256", "a9797717b63954c4d41e98febac0b0e65b2708af7d48ee4d40b6d0e092f2ad8c"},
    {"supportInfo", {
        {"int64Mode", false},
        {"staticKey", "404d0b7a58ef287687a3d011e31ac4a11b1dabab81260be2655418ec1f2a7430"},
        {"inputs", {
            {
                {"name", "x1"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            },
            {
                {"name", "x2"},
                {"index", 1},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }}},
        {"outputs", {
            {
                {"name", "y"},
                {"index", 0},
                {"dtype", "int64"},
                {"format", "ND"},
                {"paramType", "required"},
                {"shape", {-2}},
                {"format_match_mode", "FormatAgnostic"}
            }
        }},
        {"deterministic", "ignore"}
    }}
};
#endif
