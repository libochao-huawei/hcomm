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

    /**
     * @brief 记录跨 superPod retry 开关。
     * @param isRetryEnable 是否开启跨 superPod retry。
     * @return HcclResult
     *
     * @note OXC 2.0 parser 当前阶段仍以 schema 解析为主，尚未直接消费该开关；
     *       这里保留 setter 是为了与 parser 工厂注入链路保持一致，避免 2.0
     *       在不同入口下丢失运行时配置语义。
     */
    HcclResult SetIsInterSuperPodRetryEnable(bool isRetryEnable) override;

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
     * @brief 解析 A3 world-comm 主线所需的 rank 级扩展字段。
     * @param rankObj 输入 json rank 对象。
     * @param rankInfo 输出内部 rank 信息。
     * @return HcclResult
     *
     * @note 这些字段对 OXC 通用 parser 不是全部强制项，但在 910_93(A3)
     *       主线中属于关键承载信息：
     *       - `super_pod_id`
     *       - `super_device_id`
     *       - `host_ip`
     *       - `device_ip`
     *       - `backup_device_ip`
     *       - `device_port`
     *       - `backup_device_port`
     */
    HcclResult ParseA3RankExtensions(const nlohmann::json &rankObj, RankInfo_t &rankInfo);

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
     * @brief 校验 OXC level_list 的层级与 L2 约束。
     * @param levelInfo 当前层级信息。
     * @param previousNetLayer 输入输出：上一层 netLayer，初始值为 `INVALID_UINT`。
     * @return HcclResult
     *
     * @note 当前阶段仅补 OXC schema 的关键约束：
     *       1. `net_layer` 必须在 0~3 范围内；
     *       2. `level_list` 必须严格递增；
     *       3. `net_layer == 2` 时必须满足 `net_type == "OXC_Mesh"`
     *          且 `net_instance_id == "0"`。
     */
    HcclResult ValidateLevelInfo(const RankLevelInfoOxc &levelInfo, u32 &previousNetLayer);

    /**
     * @brief 校验 rank 间 level_list 的完整性与公共布局一致性。
     * @param rankList 已解析完成的 rank 列表。
     * @return HcclResult
     *
     * @note 当前阶段要求 OXC ranktable 显式包含 L0/L1/L2/L3 四层，且所有 rank
     *       的层级序列与各层 net_type 保持一致，避免 parser 输出出现在 rank 间“结构漂移”。
     */
    HcclResult ValidateRankLevelLayouts(const std::vector<RankInfo_t> &rankList);

    /**
     * @brief 校验 A3 OXC rank_list 中 superPod / superDeviceId 的一致性，并计算 superPodNum。
     * @param rankTable 输入输出：待补充 superPod 统计信息的 ranktable。
     * @return HcclResult
     */
    HcclResult ValidateAndFinalizeA3RankInfo(RankTable_t &rankTable);

    /**
     * @brief 解析单个 level 下的 rank_addr_list。
     * @param levelObj 输入 json level 对象。
     * @param levelInfo 输出内部 level 信息。
     * @param defaultAddrType level 级别缺省地址类型。
     * @return HcclResult
     */
    HcclResult ParseRankAddrList(const nlohmann::json &levelObj, RankLevelInfoOxc &levelInfo,
        const std::string &defaultAddrType);

    bool isInterSuperPodRetryEnable_ { GetExternalInputInterSuperPodRetryEnable() };
};
} // namespace hccl

#endif // TOPOINFO_RANKTABLE_OXC_H
