/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "sim_log.h"

#include <cstdint>
#include <fstream>

#include "spdlog/details/os-inl.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "zlib.h"

spdlog::logger* g_logger{nullptr};

std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> InitConsoleSink(const LogConfig& config)
{
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink->set_level(static_cast<spdlog::level::level_enum>(config.consoleLevel));
    sink->set_pattern("[%l][PID:%P][TID:%t][%s][%!] %v");
    return sink;
}

std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> InitFileSink(const LogConfig& config)
{
    // after close handler
    spdlog::file_event_handlers handlers;
    handlers.after_close = [config](const std::string& filePath) {
        // rename
        static std::atomic<uint32_t> g_log_file_index{0};
        if (filePath.size() < config.fileSuffix.size() ||
            filePath.substr(filePath.size() - config.fileSuffix.size()) != config.fileSuffix) {
            std::cout << "[ERROR] Log file name error!" << std::endl;
            return;
        }
        std::ostringstream oss;
        oss << filePath.substr(0, filePath.size() - config.fileSuffix.size())
            << "_" << std::to_string(g_log_file_index.fetch_add(1, std::memory_order_relaxed))
            << config.fileSuffix;
        const std::string newPath = oss.str();
        if (spdlog::details::os::rename(filePath, newPath) != 0) {
            std::cout << "[ERROR] Fail to rename rotating log file: " << newPath << std::endl;
            return;
        }

    };

    // log file name
    std::ostringstream logFileName;
    logFileName << config.filePath << "/" << config.fileBaseName << "_" << std::to_string(getpid()) << config.fileSuffix;

    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFileName.str(), config.maxFileSize, config.maxFiles, true, handlers);
    sink->set_level(static_cast<spdlog::level::level_enum>(config.fileLevel));
    sink->set_pattern("[%Y-%m-%d %H:%M:%S.%f][%l][PID:%P][TID:%t][%s:%#][%!] %v");
    return sink;
}

void InitLogger(const LogConfig& config)
{
    try {
        auto console_sink = InitConsoleSink(config);
        auto file_sink = InitFileSink(config);
        g_logger = new spdlog::logger("muti-logger", spdlog::sinks_init_list({console_sink, file_sink}));
        g_logger->set_level(spdlog::level::trace);  // global级别设置为最低, 只通过sink的级别控制日志输出
        g_logger->flush_on(spdlog::level::warn);

        std::atexit([] (){ DeInitLogger(); });
    } catch (...) {
        std::cout << "[ERROR] Logger init failed" << std::endl;
    }
}

void FlushLog()
{
    if (g_logger != nullptr) {
        g_logger->flush();
    }
}

void DeInitLogger()
{
    if (g_logger != nullptr) {
        delete g_logger;
        g_logger = nullptr;
    }
}
