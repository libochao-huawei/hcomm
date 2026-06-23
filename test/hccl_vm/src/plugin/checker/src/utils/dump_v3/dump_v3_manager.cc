/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dump_v3/dump_v3_manager.h"

#include <fstream>

#include "file_utils.h"
#include "sim_log.h"
#include "setting_manager.h"

namespace {
const std::string kDataDirName = "data";
const std::string kInsightDirName = "insight";
}

namespace HcclSim {
HcclResult DumpV3Manager::Initialize(const std::string &dataId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (dataId.empty()) {
        HCCL_VM_ERROR("[DumpV3Manager::Initialize] dataId is empty.");
        return HcclResult::HCCL_E_PARA;
    }

    m_dataId = dataId;
    m_pluginRootDir = HcclSim::GetCurrentPath();
    if (m_pluginRootDir.empty()) {
        HCCL_VM_ERROR("[DumpV3Manager::Initialize] failed to get current path.");
        return HcclResult::HCCL_E_INTERNAL;
    }

    m_dataDir = HcclSim::JoinPath(m_pluginRootDir, kDataDirName);
    m_rootDir = HcclSim::JoinPath(HcclSim::JoinPath(m_dataDir, kInsightDirName), m_dataId);

    if (!SettingManager::GetInstance().IsInsightDumpEnabled()) {
        HCCL_VM_INFO("[DumpV3Manager] insight dump disabled by manifest setting.");
        return HcclResult::HCCL_SUCCESS;
    }
    return PrepareDirs();
}

void DumpV3Manager::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dataId.clear();
    m_pluginRootDir.clear();
    m_dataDir.clear();
    m_rootDir.clear();
}

bool DumpV3Manager::IsEnabled() const
{
    return SettingManager::GetInstance().IsInsightDumpEnabled();
}

std::string DumpV3Manager::GetRootDir() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rootDir;
}

std::string DumpV3Manager::GetOutputPath(const std::string &relativePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return HcclSim::JoinPath(m_rootDir, relativePath);
}

HcclResult DumpV3Manager::WriteMsgpack(const std::string &relativePath, const nlohmann::json &document) const
{
    if (!IsEnabled()) {
        return HcclResult::HCCL_SUCCESS;
    }
    const std::string fullPath = GetOutputPath(relativePath);
    auto ret = HcclSim::EnsureDirectory(HcclSim::GetParentPath(fullPath));
    if (ret != HcclResult::HCCL_SUCCESS) {
        return ret;
    }

    std::ofstream out(fullPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        HCCL_VM_ERROR("[DumpV3Manager::WriteMsgpack] failed to open file: {}", fullPath);
        return HcclResult::HCCL_E_INTERNAL;
    }

    try {
        nlohmann::json::to_msgpack(document, nlohmann::detail::output_adapter<char>(out));
    } catch (const std::exception &ex) {
        HCCL_VM_ERROR("[DumpV3Manager::WriteMsgpack] msgpack serialize failed: {}, file: {}", ex.what(),
            fullPath);
        return HcclResult::HCCL_E_INTERNAL;
    }

    out.flush();
    if (!out.good()) {
        HCCL_VM_ERROR("[DumpV3Manager::WriteMsgpack] failed to flush file: {}", fullPath);
        return HcclResult::HCCL_E_INTERNAL;
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult DumpV3Manager::WriteJson(const std::string &relativePath, const nlohmann::json &document) const
{
    if (!IsEnabled()) {
        return HcclResult::HCCL_SUCCESS;
    }
    const std::string fullPath = GetOutputPath(relativePath);
    auto ret = HcclSim::EnsureDirectory(HcclSim::GetParentPath(fullPath));
    if (ret != HcclResult::HCCL_SUCCESS) {
        return ret;
    }

    std::ofstream out(fullPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        HCCL_VM_ERROR("[DumpV3Manager::WriteJson] failed to open file: {}", fullPath);
        return HcclResult::HCCL_E_INTERNAL;
    }

    try {
        out << document.dump(2) << std::endl;
    } catch (const std::exception &ex) {
        HCCL_VM_ERROR("[DumpV3Manager::WriteJson] json serialize failed: {}, file: {}", ex.what(), fullPath);
        return HcclResult::HCCL_E_INTERNAL;
    }

    if (!out.good()) {
        HCCL_VM_ERROR("[DumpV3Manager::WriteJson] failed to write file: {}", fullPath);
        return HcclResult::HCCL_E_INTERNAL;
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult DumpV3Manager::PrepareDirs() const
{
    auto ret = HcclSim::EnsureDirectory(m_dataDir);
    if (ret != HcclResult::HCCL_SUCCESS) {
        return ret;
    }
    ret = HcclSim::EnsureDirectory(HcclSim::JoinPath(m_dataDir, kInsightDirName));
    if (ret != HcclResult::HCCL_SUCCESS) {
        return ret;
    }
    return HcclSim::EnsureDirectory(m_rootDir);
}
}  // namespace HcclSim
