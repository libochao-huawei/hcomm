/**
 * Copyright 2019-2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fstream>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <sstream>
#include <stdio.h>
#include <cstdarg>
#include <securec.h>
#include <vector>

using namespace std;

namespace error_message {
    std::string TrimPath(const std::string &str);
    struct Context {
        uint64_t work_stream_id;
        std::string first_stage;
        std::string second_stage;
        std::string log_header;
    };

void ReportInnerError(const char *file_name, const char *func, uint32_t line, const std::string error_code, const char *format, ...) {
  return;
}

int32_t ReportInnerErrMsg(const char *file_name, const char *func, uint32_t line, const char *error_code,
                          const char *format, ...) {
  return 0;
}

int32_t ReportPredefinedErrMsg(const char *error_code, const std::vector<const char *> &key,
                               const std::vector<const char *> &value) {
  return 0;            
}
}

using namespace error_message;

class ErrorManager {
 public:
  static ErrorManager &GetInstance();

  void ATCReportErrMessage(std::string error_code, const std::vector<std::string> &key = {},
                           const std::vector<std::string> &value = {});

  int ReportInterErrMessage(std::string error_code, const std::string &error_msg);

  const std::string &GetLogHeader();

  void SetErrorContext(error_message::Context error_context);

  ErrorManager() {}
  ~ErrorManager() {}

  ErrorManager(const ErrorManager &) = delete;
  ErrorManager(ErrorManager &&) = delete;
  ErrorManager &operator=(const ErrorManager &) = delete;
  ErrorManager &operator=(ErrorManager &&) = delete;
};

ErrorManager &ErrorManager::GetInstance() {
  static ErrorManager instance;
  return instance;
}

void ErrorManager::SetErrorContext(error_message::Context error_context)
{
    return;
}

void ErrorManager::ATCReportErrMessage(std::string error_code, const std::vector<std::string> &key,
    const std::vector<std::string> &value)
{
    return;
}

int ErrorManager::ReportInterErrMessage(std::string error_code, const std::string &error_msg)
{
    return 0;
}
