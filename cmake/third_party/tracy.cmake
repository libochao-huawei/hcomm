# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

include_guard(GLOBAL)

if(NOT ENABLE_TRACY)
    message(STATUS "[ThirdParty] Tracy disabled by ENABLE_TRACY=OFF")
    return()
endif()

set(TRACY_SRC_PATH ${PROJECT_SOURCE_DIR}/tools/tracy_src)
set(TRACY_CLIENT_SOURCE ${TRACY_SRC_PATH}/public/TracyClient.cpp)
set(TRACY_PUBLIC_INCLUDE ${TRACY_SRC_PATH}/public)

if(NOT EXISTS ${TRACY_CLIENT_SOURCE})
    message(FATAL_ERROR "[ThirdParty] Offline Tracy source not found: ${TRACY_CLIENT_SOURCE}.")
endif()

# Collect compile sources: TracyClient.cpp is the unity build entry point.
set(TRACY_COMPILE_SOURCES ${TRACY_CLIENT_SOURCE})
set(TRACY_EXTRA_INCLUDE_DIRS "")
set(TRACY_EXTRA_DEFINITIONS "")

# TRACY_SAVE_NO_SEND: offline ring-buffer export (Chrome JSON / Perfetto).
# TracyLiteAll.cpp is #include-d by TracyClient.cpp under this guard — no
# separate compile unit needed; we only validate the file exists.
if(TRACY_SAVE_NO_SEND)
    set(TRACY_LITE_ALL ${TRACY_SRC_PATH}/public/client/TracyLiteAll.cpp)
    if(NOT EXISTS ${TRACY_LITE_ALL})
        message(FATAL_ERROR "[ThirdParty] TracyLiteAll.cpp missing: ${TRACY_LITE_ALL}. "
                            "Sync from commit ec6fc008415dd0fedc339835a5e4a911bdd330f6.")
    endif()
    list(APPEND TRACY_EXTRA_DEFINITIONS TRACY_SAVE_NO_SEND)
    message(STATUS "[ThirdParty] Tracy SAVE_NO_SEND enabled (offline export)")
endif()

# TRACYLITE_PERFETTO: Perfetto native trace format export.
# Uses protozero-only fast path; no Perfetto runtime threads/IPC.
# TracyLitePerfetto.cpp is compiled as a separate TU (NOT included as .cpp #include).
if(TRACYLITE_PERFETTO)
    set(TRACY_PERFETTO_SDK ${TRACY_SRC_PATH}/perfetto_sdk)
    set(TRACY_LITE_PERFETTO ${TRACY_SRC_PATH}/public/client/TracyLitePerfetto.cpp)
    if(NOT EXISTS ${TRACY_PERFETTO_SDK}/perfetto.h)
        message(FATAL_ERROR "[ThirdParty] Perfetto SDK missing: ${TRACY_PERFETTO_SDK}/perfetto.h. "
                            "Sync from commit ec6fc008415dd0fedc339835a5e4a911bdd330f6.")
    endif()
    if(NOT EXISTS ${TRACY_LITE_PERFETTO})
        message(FATAL_ERROR "[ThirdParty] TracyLitePerfetto.cpp missing: ${TRACY_LITE_PERFETTO}. "
                            "Sync from commit ec6fc008415dd0fedc339835a5e4a911bdd330f6.")
    endif()
    # perfetto.cc: amalgamated Perfetto SDK implementation (protozero + trace packet serialization)
    # TracyLitePerfetto.cpp: provides PerfettoNativeExporter class for exporting to .perfetto-trace
    list(APPEND TRACY_COMPILE_SOURCES ${TRACY_PERFETTO_SDK}/perfetto.cc ${TRACY_LITE_PERFETTO})
    list(APPEND TRACY_EXTRA_INCLUDE_DIRS ${TRACY_PERFETTO_SDK})
    list(APPEND TRACY_EXTRA_DEFINITIONS TRACYLITE_PERFETTO)
    message(STATUS "[ThirdParty] Tracy Perfetto export enabled")
endif()

# Build Tracy as a STATIC library so that project-wide -Werror is NOT applied
# to third-party source files. Warning flags are set only on this target.
if(NOT TARGET tracy_client)
    add_library(tracy_client STATIC ${TRACY_COMPILE_SOURCES})
endif()

target_include_directories(tracy_client PUBLIC ${TRACY_PUBLIC_INCLUDE} ${TRACY_EXTRA_INCLUDE_DIRS})
target_compile_definitions(tracy_client PUBLIC TRACY_ENABLE ${TRACY_EXTRA_DEFINITIONS})

# Suppress third-party compiler warnings without touching project -Werror policy.
target_compile_options(tracy_client PRIVATE
    -Wno-error
    -Wno-sign-compare
    -Wno-format
    -Wno-unused-result
    -Wno-unused-function
    -Wno-maybe-uninitialized
)

if(UNIX)
    target_link_libraries(tracy_client PUBLIC pthread ${CMAKE_DL_LIBS})
endif()

set(TRACY_SRC_INCLUDE ${TRACY_PUBLIC_INCLUDE})
message(STATUS "[ThirdParty] Tracy enabled with source integration: ${TRACY_CLIENT_SOURCE}")
