/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

#ifndef TOPOINFO_RANKTABLE_OXC_H
#define TOPOINFO_RANKTABLE_OXC_H

#include "topoinfo_ranktableConcise.h"

namespace hccl {

/**
* @brief OXC 组网 RankTable 解析器
* @details 继承自 TopoinfoRanktableConcise，复用大部分逻辑，
*          主要处理 OXC 特有的字段和层级变化
*
* OXC RankTable 关键特性：
* 1. 版本号：2.0
* 2. 新增 task_id 字段
* 3. NetLayer 0: mesh (TOPO_FILE_DESC)
* 4. NetLayer 1: 框内 CLOS (net_instance_id 为 Group_ID)
* 5. NetLayer 2: 超平面 OXC_Mesh (net_instance_id 固定为 "0")
* 6. NetLayer 3: 参数面 CLOS
*/
class TopoinfoRanktableOxc : public TopoinfoRanktableConcise {
public:
    explicit TopoinfoRanktableOxc(const std::string &rankTableM, const std::string &identify);
    ~TopoinfoRanktableOxc() override = default;

    /**
    * @brief 初始化 OXC RankTable 解析器
    * @return HCCL_SUCCESS 成功
    */
    HcclResult Init() override;

    /**
    * @brief 获取集群信息（OXC 版本）
    * @param params 输出参数
    * @param rankTable 输出 RankTable
    * @return HCCL_SUCCESS 成功
    */
    HcclResult GetClusterInfo(hccl::HcclCommParams &params,
        hccl::RankTable_t &rankTable) override;

protected:
    /**
    * @brief 解析 OXC RankTable 的 rank_list 格式
    * @param rankTable 输出 RankTable
    * @return HCCL_SUCCESS 成功
    */
    HcclResult ParseOxcRankTable(RankTable_t &rankTable);

    /**
    * @brief 解析 task_id 字段（OXC 特有）
    * @param obj JSON 对象
    * @param taskId 输出任务 ID
    * @return HCCL_SUCCESS 成功
    */
    HcclResult ParseTaskId(const nlohmann::json &obj, std::string &taskId);

    std::string taskId_;  // OXC 任务 ID
};

} // namespace hccl

#endif // TOPOINFO_RANKTABLE_OXC_H
