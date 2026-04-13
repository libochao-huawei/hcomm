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

#include <string>

#include "hccl/base.h"
#include "hccl_types.h"
#include "topoinfo_ranktableParser_pub.h"

namespace hccl {
/**
 * @brief OXC ranktable 2.0 的专用 parser。
 *
 * @note 第一阶段目标是把 OXC schema 解析为内部 `params + rankTable`，
 *       因此该类只负责“读取/校验/保存”，不承担下游拓扑消费逻辑。
 */
class TopoinfoRanktableOxc : public TopoInfoRanktableParser {
public:
    /**
     * @brief 构造 OXC ranktable parser。
     * @param rankTableM 输入 ranktable 字符串。
     * @param identify 当前 rank 的字符串编号。
     */
    explicit TopoinfoRanktableOxc(const std::string &rankTableM, const std::string &identify);
    ~TopoinfoRanktableOxc() override;

    /**
     * @brief 初始化 parser，并完成一次完整解析。
     * @return HcclResult
     * @retval HCCL_SUCCESS 解析成功。
     * @retval 其他 载入或解析失败。
     */
    HcclResult Init() override;

    /**
     * @brief 导出内部缓存的 ranktable。
     * @param clusterInfo 输出 ranktable。
     * @return HcclResult
     */
    HcclResult GetClusterInfo(RankTable_t &clusterInfo) override;

    /**
     * @brief 导出内部缓存的 params 与 ranktable。
     * @param params 输出通信域初始化参数。
     * @param rankTable 输出 ranktable。
     * @return HcclResult
     */
    HcclResult GetClusterInfo(HcclCommParams &params, RankTable_t &rankTable) override;

private:
    TopoinfoRanktableOxc(const TopoinfoRanktableOxc &);
    TopoinfoRanktableOxc &operator=(const TopoinfoRanktableOxc &);

    /**
     * @brief 执行 OXC ranktable 主解析流程。
     * @param params 输出通信域初始化参数。
     * @param rankTable 输出 ranktable。
     * @return HcclResult
     */
    HcclResult ParserClusterInfo(HcclCommParams &params, RankTable_t &rankTable);

    /**
     * @brief 解析 OXC 2.0 的可选字段 task_id。
     * @param rankTable 输出 ranktable。
     * @return HcclResult
     * @note task_id 缺失时保留空串，不影响第一阶段解析完成标准。
     */
    HcclResult ParseTaskId(RankTable_t &rankTable);

    /**
     * @brief 解析 OXC 2.0 的 rank_list。
     * @param rankTable 输出 ranktable。
     * @return HcclResult
     */
    HcclResult ParseRankList(RankTable_t &rankTable);

    /**
     * @brief 解析单个 rank 对象。
     * @param rankObj 输入 json rank 对象。
     * @param rankInfo 输出内部 rank 信息。
     * @return HcclResult
     */
    HcclResult ParseSingleRank(const nlohmann::json &rankObj, RankInfo_t &rankInfo);

    /**
     * @brief 解析 rank 对象中的 level_list。
     * @param rankObj 输入 json rank 对象。
     * @param rankInfo 输出内部 rank 信息。
     * @return HcclResult
     * @note level_list / rank_addr_list 当前只做最小承载，
     *       不在 parser 阶段引入 CommPlane/RankGraph 语义。
     */
    HcclResult ParseLevelList(const nlohmann::json &rankObj, RankInfo_t &rankInfo);

    /**
     * @brief 解析单个 level 下的 rank_addr_list。
     * @param levelObj 输入 json level 对象。
     * @param levelInfo 输出内部 level 信息。
     * @param defaultAddrType level 级别缺省地址类型。
     * @return HcclResult
     */
    HcclResult ParseRankAddrList(const nlohmann::json &levelObj, RankLevelInfoOxc &levelInfo,
        const std::string &defaultAddrType);
};
} // namespace hccl

#endif // TOPOINFO_RANKTABLE_OXC_H
