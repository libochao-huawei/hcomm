# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set -e

CURRENT_DIR=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
BUILD_DIR=${CURRENT_DIR}/build
BUILD_DEVICE_DIR="${CURRENT_DIR}/build_device"
BUILD_OUTPUT_DIR=${CURRENT_DIR}/build_out
OUTPUT_PATH="${CURRENT_DIR}/output"
USER_ID=$(id -u)
CPU_NUM=$(cat /proc/cpuinfo | grep "^processor" | wc -l)
JOB_NUM="-j${CPU_NUM}"
ASAN="false"
COV="false"
CUSTOM_OPTION="-DCMAKE_INSTALL_PREFIX=${BUILD_OUTPUT_DIR}"
FULL_MODE="false"  # 新增变量，用于控制是否全量构建
KERNEL="false"  # 新增变量，用于控制是否只编译 ccl_kernel.so
DO_NOT_CLEAN="false" # 是否清理
CANN_3RD_LIB_PATH="${CURRENT_DIR}/third_party"
CANN_UTILS_LIB_PATH="${CURRENT_DIR}/utils"
BUILD_AARCH="false"
CUSTOM_SIGN_SCRIPT="${CURRENT_DIR}/scripts/sign/community_sign_build.py"
ENABLE_SIGN="false"
VERSION_INFO="8.5.0"

BUILD_FWK_HLT="false"
MOCK_FWK_HLT="0"

BUILD_CB_TEST="false"

ENABLE_UT="off"
ENABLE_ST="off"
ST_TASKS=()
ENABLE_GCOV="off"
ENABLE_NO_EXEC="off"
CMAKE_BUILD_TYPE="Debug"

if [ "${USER_ID}" != "0" ]; then
    DEFAULT_TOOLKIT_INSTALL_DIR="${HOME}/Ascend/ascend-toolkit/latest"
    DEFAULT_INSTALL_DIR="${HOME}/Ascend/latest"
else
    DEFAULT_TOOLKIT_INSTALL_DIR="/usr/local/Ascend/ascend-toolkit/latest"
    DEFAULT_INSTALL_DIR="/usr/local/Ascend/latest"
fi

function log() {
    local current_time=`date +"%Y-%m-%d %H:%M:%S"`
    echo "[$current_time] "$1
}

function set_env()
{
    source $ASCEND_CANN_PACKAGE_PATH/set_env.sh || echo "0"
}

function clean()
{
    if [ -n "${BUILD_DIR}" ];then
        rm -rf ${BUILD_DIR}
    fi

    if [ -z "${TEST}" ] && [ -z "${KERNEL}" ];then
        if [ -n "${BUILD_OUTPUT_DIR}" ];then
            rm -rf ${BUILD_OUTPUT_DIR}
        fi
    fi

    mkdir -p ${BUILD_DIR}
}

function rmdir()
{
    if [ "${DO_NOT_CLEAN}" = "false" ] && [ $# -gt 0 ]; then
        rm -rf "$@"
    fi
}

function cmake_config()
{
    log "Info: cmake config ${CUSTOM_OPTION} $*"
    cmake .. ${CUSTOM_OPTION} "$@"
}

function build()
{
    log "Info: build target:$@ JOB_NUM:${JOB_NUM}"
    cmake --build . --target "$@" ${JOB_NUM} #--verbose
}

function build_package(){
    cmake_config
    log "Info: build_package"
    build package
}

function build_device(){
    cmake_config
    log "Info: build_device"
    TARGET_LIST="hccp_service.bin rs net_co ccl_kernel_plf ccl_kernel_plf_a ccl_kernel aicpu_custom_json aicpu_custom"
    echo "TARGET_LIST=${TARGET_LIST}"
    PKG_TARGET_LIST="generate_device_hccp_package generate_device_aicpu_package"
    echo "PKG_TARGET_LIST=${PKG_TARGET_LIST}"
    SIGN_TARGET_LIST="sign_cann_hcomm_compat sign_aicpu_hcomm"
    echo "SIGN_TARGET_LIST=${SIGN_TARGET_LIST}"
    build ${TARGET_LIST} ${PKG_TARGET_LIST} ${SIGN_TARGET_LIST}
}

function build_hccd(){
    cmake_config
    log "Info: build_hccd"
    TARGET_LIST="rs ra_peer ra_hdc ra hccd"
    echo "TARGET_LIST=${TARGET_LIST}"
    PKG_TARGET_LIST="generate_device_hccd_package"
    echo "PKG_TARGET_LIST=${PKG_TARGET_LIST}"
    SIGN_TARGET_LIST="sign_cann_hccd_compat"
    echo "SIGN_TARGET_LIST=${SIGN_TARGET_LIST}"
    build ${TARGET_LIST} ${PKG_TARGET_LIST} ${SIGN_TARGET_LIST}
}

function build_cb_test_verify(){
    cd ${CURRENT_DIR}/examples/
    bash build.sh
}

function run_ctest() {
    # 设置 --noexec 选项，则跳过执行测试用例
    if [[ "$ENABLE_NO_EXEC" = "on" ]]; then
        log "Info: Skip executing tests"
        return 0
    fi

    local suite_name="$1"   # "ut" or "st"
    local log_dir="${OUTPUT_PATH}/logs/${suite_name}"
    local ctest_log="${log_dir}/ctest_output.log"
    local ctest_summary="${log_dir}/ctest_summary.log"

    # 创建日志目录
    mk_dir "${log_dir}"

    # CTest 执行用例
    ctest ${JOB_NUM} \
          --build-nocmake \
          --timeout 200 \
          --output-on-failure \
          --stop-on-failure \
          --test-output-size-failed 10000000 \
          -O "${ctest_log}" \
          2>&1 | tee "${ctest_summary}"

    # 超时时间：200s
    local ctest_ret=${PIPESTATUS[0]}
    if [ "${ctest_ret}" -eq 137 ]; then
        log "Error: ctest timeout: execute more than 200s killed"
        exit 1
    fi

    return ${ctest_ret}
}

function build_st() {
    log "Info: build_st"
    mk_dir "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    # 配置 ST 用例代码
    local st_tasks=$(printf '%s;' "${ST_TASKS[@]}" | sed 's/;$//')
    log "Info: build_st: ST_TASKS=${st_tasks}"
    cmake_config -DPRODUCT_SIDE=host \
                 -DENABLE_GCOV=${ENABLE_GCOV} \
                 -DENABLE_TEST=${ENABLE_TEST} \
                 -DENABLE_ST=${ENABLE_ST} \
                 -DST_TASKS=${st_tasks}
    if [ $? -ne 0 ]; then
        log "Error: build_st: cmake config failed"
        exit 1
    fi

    # 编译 ST 用例代码
    cmake --build . ${JOB_NUM}
    if [ $? -ne 0 ]; then
        log "Error: build_st: cmake build failed"
        exit 1
    fi

    # CTest 运行用例
    run_ctest "st"
    if [ $? -ne 0 ]; then
        log "Error: build_st: ctest execution failed"
        exit 1
    fi

    log "Info: Build and tests completed successfully!"
}

function build_kernel() {
    cmake_config
    log "Info: build_kernel"
    build ccl_kernel_plf ccl_kernel_plf_a ccl_kernel aicpu_custom_json aicpu_custom
}

function mk_dir() {
  local create_dir="$1"  # the target to make
  mkdir -pv "${create_dir}"
  echo "created ${create_dir}"
}

function build_ut() {
    log "Info: build_ut"
    mk_dir "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    # 避免加载系统库
    unset LD_LIBRARY_PATH

    # 配置 UT 用例代码
    cmake_config -DPRODUCT_SIDE=host \
                 -DENABLE_TEST=${ENABLE_TEST} \
                 -DENABLE_UT=${ENABLE_UT} \
                 -DENABLE_GCOV=${ENABLE_GCOV}
    if [ $? -ne 0 ]; then
        log "Error: build_ut: cmake config failed"
        exit 1
    fi

    # 编译 UT 用例代码
    cmake --build . ${JOB_NUM}
    if [ $? -ne 0 ]; then
        log "Error: build_ut: cmake build failed"
        exit 1
    fi

    # CTest 运行用例
    run_ctest "ut"
    if [ $? -ne 0 ]; then
        log "Error: build_ut: ctest execution failed"
        exit 1
    fi

    log "Info: Build and tests completed successfully!"
}

function make_ut_gov() {
# Detect lcov version and set appropriate ignore errors flags
    detect_lcov_flags() {
        LCOV_VERSION=$(lcov --version 2>/dev/null | head -n1 | sed 's/.*LCOV version //' | cut -d. -f1)
        if [[ "${LCOV_VERSION}" -ge 2 ]]; then
            LCOV_CAPTURE_IGNORE_FLAGS="--ignore-errors mismatch,corrupt,empty,inconsistent,negative,unused"
            LCOV_FILTER_IGNORE_FLAGS="--ignore-errors unused"
            LCOV_RUNTIME_CONFIGURATION_FLAGS="-rc geninfo_unexecuted_blocks=1"
        else
            LCOV_CAPTURE_IGNORE_FLAGS=""
            LCOV_FILTER_IGNORE_FLAGS=""
            LCOV_RUNTIME_CONFIGURATION_FLAGS=""
        fi
    }

  if [[ "X$ENABLE_UT" = "Xon" && "X$ENABLE_GCOV" = "Xon" ]]; then
    detect_lcov_flags
    echo "Generated coverage statistics, please wait..."
    cd ${CURRENT_DIR}
    rm -rf ${CURRENT_DIR}/cov
    mkdir -p ${CURRENT_DIR}/cov

    lcov -c ${LCOV_CAPTURE_IGNORE_FLAGS} \
         -d ${BUILD_DIR}/test/ut/ \
         -d ${BUILD_DIR}/test/legacy/ut/ \
         -o cov/coverage.info \
         ${LCOV_RUNTIME_CONFIGURATION_FLAGS}
    lcov ${LCOV_FILTER_IGNORE_FLAGS} -r cov/coverage.info */src/platform/hccp/external_depends/* -o cov/coverage.info
    lcov ${LCOV_FILTER_IGNORE_FLAGS} -e cov/coverage.info */src/algorithm/* */src/common/* */src/hccd/* */src/legacy/* */src/framework/* */src/platform/* */src/pub_inc/* -o cov/coverage.info

    cd ${CURRENT_DIR}/cov
    genhtml coverage.info
  fi
}

# print usage message
function usage() {
  echo "Usage:"
  echo "  sh build.sh --pkg [-h | --help] [-j<N>]"
  echo "              [--cann_3rd_lib_path=<PATH>] [-p|--package-path <PATH>]"
  echo "              [--asan]"
  echo "              [--sign-script <PATH>] [--enable-sign] [--version <VERSION>]"
  echo ""
  echo "Options:"
  echo "    -h, --help     Print usage"
  echo "    --asan         Enable AddressSanitizer"
  echo "    --build-type=<TYPE>"
  echo "                   Specify build type (TYPE options: Release/Debug), Default: Release"
  echo "    -j<N>          Set the number of threads used for building, default is 8"
  echo "    --cann_3rd_lib_path=<PATH>"
  echo "                   Set ascend third_party package install path, default ./output/third_party"
  echo "    -p, --package-path <PATH>"
  echo "                   Set ascend package install path, default /usr/local/Ascend/cann"
  echo "    --sign-script <PATH>"
  echo "                   Set sign-script's path to <PATH>"
  echo "    --enable-sign"
  echo "                   Enable to sign"
  echo "    --version <VERSION>"
  echo "                   Set sign version to <VERSION>"
  echo "    -u, --ut       Run all unit tests (UT)"
  echo "    -s, --st       Run all system tests (ST)"
  echo "    --noexec       Run build and skip executing tests"
  echo ""
}

while [[ $# -gt 0 ]]; do
    case "$1" in
      -h | --help)
        usage
        exit 0
        ;;
    -j*)
        JOB_NUM="$1"
        shift
        ;;
    --build-type=*)
        OPTARG=$1
        BUILD_TYPE="${OPTARG#*=}"
        shift
        ;;
    --ccache)
        CCACHE_PROGRAM="$2"
        shift 2
        ;;
    -p|--package-path)
        ascend_package_path="$2"
        shift 2
        ;;
    --nlohmann_path)
        third_party_nlohmann_path="$2"
        shift 2
        ;;
    --pkg)
        # 跳过 --pkg，不做处理
        shift
        ;;
    --cann_3rd_lib_path=*)
        OPTARG=$1
        CANN_3RD_LIB_PATH="$(realpath ${OPTARG#*=})"
        shift
        ;;
    --noexec)
        ENABLE_NO_EXEC="on"
        shift
        ;;
    -u|--ut)
        ENABLE_TEST="on"
        ENABLE_UT="on"
        shift
        ;;
    -s|--st)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("all")
        shift
        ;;
    --open_hccl_test)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("open_hccl_test")
        shift
        ;;
    --executor_hccl_test)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("executor_hccl_test")
        shift
        ;;
    --executor_reduce_hccl_test)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("executor_reduce_hccl_test")
        shift
        ;;
    --executor_pipeline_hccl_test)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("executor_pipeline_hccl_test")
        shift
        ;;
    --legacy_all_testcase)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("legacy_all_testcase")
        shift
        ;;
    --legacy_aicpu_2d_testcase)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("legacy_aicpu_2d_testcase")
        shift
        ;;
    --legacy_ccu_2d_testcase)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("legacy_ccu_2d_testcase")
        shift
        ;;
    --legacy_ccu_1d_hf16p_testcase)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("legacy_ccu_1d_hf16p_testcase")
        shift
        ;;
    --legacy_ccu_1d_testcase_part1)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("legacy_ccu_1d_testcase_part1")
        shift
        ;;
    --legacy_ccu_1d_testcase_part2)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("legacy_ccu_1d_testcase_part2")
        shift
        ;;
    --legacy_alg_ccu_reduce)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("legacy_alg_ccu_reduce")
        shift
        ;;
    --legacy_function_ut_testcase)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("legacy_function_ut_testcase")
        shift
        ;;
    --legacy_alg_testcase)
        ENABLE_TEST="on"
        ENABLE_ST="on"
        ST_TASKS+=("legacy_alg_testcase")
        shift
        ;;
    --aicpu)  # 新增选项，用于只编译 ccl_kernel.so
        KERNEL="true"
        shift
        ;;
    --full)
        FULL_MODE="true"
        shift
        ;;
    --build_aarch)
        BUILD_AARCH="true"
        shift
        ;;
    --asan)
        ASAN="true"
        shift
        ;;
    --cov)
        ENABLE_GCOV="on"
        COV="true"
        shift
        ;;
    --noclean)
        DO_NOT_CLEAN="true"
        shift
        ;;
    --fwk_test_hlt)
        BUILD_FWK_HLT="true"
        MOCK_FWK_HLT="0"
        shift
        ;;
    --fwk_test_hlt_mock)
        BUILD_FWK_HLT="true"
        MOCK_FWK_HLT="1"
        shift
        ;;
    --cb_test_verify)
        BUILD_CB_TEST="true"
        shift
        ;;
    --enable-sign)
        ENABLE_SIGN="true"
        shift
        ;;
    --sign-script)
        CUSTOM_SIGN_SCRIPT="$(realpath $2)"
        shift 2
        ;;
    --version)
        VERSION_INFO="$2"
        shift 2
        ;;
    *)
        log "Error: Undefined option: $1"
        usage
        exit 1
        ;;
    esac
done

if [ "${KERNEL}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DKERNEL_MODE=ON -DDEVICE_MODE=ON -DPRODUCT=ascend -DPRODUCT_SIDE=device"
fi

if [ "${FULL_MODE}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DFULL_MODE=ON"
else
    CUSTOM_OPTION="${CUSTOM_OPTION} -DFULL_MODE=OFF"
fi

if [ "${BUILD_AARCH}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DAARCH_MODE=ON"
else
    CUSTOM_OPTION="${CUSTOM_OPTION} -DAARCH_MODE=OFF"
fi

if [ "${ASAN}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DENABLE_ASAN=ON"
fi

if [ "${COV}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DENABLE_GCOV=ON"
fi

CUSTOM_OPTION="${CUSTOM_OPTION} -DASCENDC_DUMP=0" # AIV算子调测阶段可配置为1以打开AscendC打印功能

if [ -n "${ascend_package_path}" ];then
    ASCEND_CANN_PACKAGE_PATH=${ascend_package_path}
elif [ -n "${ASCEND_HOME_PATH}" ];then
    ASCEND_CANN_PACKAGE_PATH=${ASCEND_HOME_PATH}
elif [ -n "${ASCEND_OPP_PATH}" ];then
    ASCEND_CANN_PACKAGE_PATH=$(dirname ${ASCEND_OPP_PATH})
elif [ -d "${DEFAULT_TOOLKIT_INSTALL_DIR}" ];then
    ASCEND_CANN_PACKAGE_PATH=${DEFAULT_TOOLKIT_INSTALL_DIR}
elif [ -d "${DEFAULT_INSTALL_DIR}" ];then
    ASCEND_CANN_PACKAGE_PATH=${DEFAULT_INSTALL_DIR}
else
    log "Error: Please set the toolkit package installation directory through parameter -p|--package-path."
    exit 1
fi

if [ -n "${third_party_nlohmann_path}" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DTHIRD_PARTY_NLOHMANN_PATH=${third_party_nlohmann_path}"
fi

CUSTOM_OPTION="${CUSTOM_OPTION} -DCUSTOM_ASCEND_CANN_PACKAGE_PATH=${ASCEND_CANN_PACKAGE_PATH}"
CUSTOM_OPTION="${CUSTOM_OPTION} -DASCEND_INSTALL_PATH=${ASCEND_CANN_PACKAGE_PATH}"
# CUSTOM_OPTION="${CUSTOM_OPTION} -DCANN_3RD_LIB_PATH=${cann_3rd_lib_path}"
CUSTOM_OPTION="$CUSTOM_OPTION -DCANN_3RD_LIB_PATH=${CANN_3RD_LIB_PATH} -DCANN_UTILS_LIB_PATH=${CANN_UTILS_LIB_PATH}"

CUSTOM_OPTION="$CUSTOM_OPTION -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"

set_env

if [ "${DO_NOT_CLEAN}" = "false" ]; then
    clean
else
    mkdir -p "${BUILD_DIR}"
fi

cd ${BUILD_DIR}

if [ "${ENABLE_UT}" == "on" ]; then
    build_ut
    make_ut_gov
elif [ "${ENABLE_ST}" == "on" ]; then
    build_st 
elif [ "${KERNEL}" == "true" ]; then
    build_kernel
elif [ "${BUILD_FWK_HLT}" == "true" ]; then
    log "Info: Building fwk_test with MOCK_HCCL=${MOCK_FWK_HLT}"
    cmake ${CUSTOM_OPTION} -DMOCK_HCCL=${MOCK_FWK_HLT} ../test/hlt
    build hcomm_test
    log "Info: fwk_test execution example: ${BUILD_DIR}/hcomm_test --cluster_info test/hlt/ranktable.json --rank 0 --list"
    log "Info: fwk_test execution example: ${BUILD_DIR}/hcomm_test --cluster_info test/hlt/ranktable.json --rank 0 --test allocthread"
elif [ "${BUILD_CB_TEST}" == "true" ]; then
    log "Info: Building cb_test_verify"
    build_cb_test_verify
    if grep -q "Make Failure" ${BUILD_DIR}/build.log || grep -q "Make test Failure" ${BUILD_DIR}/build.log; then
        log "Info: Building cb_test_verify failed"
        exit 1
    else
        log "Info: Building cb_test_verify success"
    fi
elif [ "${FULL_MODE}" == "true" ]; then
    cd ..
    mkdir -p ${BUILD_DEVICE_DIR}
    cd ${BUILD_DEVICE_DIR}
    CURRENT_CUSTOM_OPTION="${CUSTOM_OPTION}"
    CUSTOM_OPTION="${CURRENT_CUSTOM_OPTION} -DFULL_MODE=ON -DDEVICE_MODE=ON -DKERNEL_MODE=ON -DPRODUCT=ascend910B -DPRODUCT_SIDE=device -DUSE_ALOG=0 -DCUSTOM_SIGN_SCRIPT=${CUSTOM_SIGN_SCRIPT} -DENABLE_SIGN=${ENABLE_SIGN} -DVERSION_INFO=${VERSION_INFO}"
    build_device
    BUILD_HCCD_DIR="${CURRENT_DIR}/build_hccd"
    mkdir -p ${BUILD_HCCD_DIR}
    cd ${BUILD_HCCD_DIR}
    CUSTOM_OPTION="${CURRENT_CUSTOM_OPTION} -DDEVICE_MODE=ON -DPRODUCT=ascend -DPRODUCT_SIDE=device -DUSE_ALOG=1 -DCUSTOM_SIGN_SCRIPT=${CUSTOM_SIGN_SCRIPT} -DENABLE_SIGN=${ENABLE_SIGN} -DVERSION_INFO=${VERSION_INFO}"
    build_hccd
    cd .. & cd ${BUILD_DIR}
    CUSTOM_OPTION="${CURRENT_CUSTOM_OPTION} -DDEVICE_MODE=OFF -DPRODUCT=ascend -DPRODUCT_SIDE=host -DUSE_ALOG=1"
    build_package
    rmdir ${BUILD_DEVICE_DIR} ${BUILD_HCCD_DIR}
else
    cd ..
    mkdir -p ${BUILD_DEVICE_DIR}
    cd ${BUILD_DEVICE_DIR}
    CURRENT_CUSTOM_OPTION="${CUSTOM_OPTION}"
    CUSTOM_OPTION="${CURRENT_CUSTOM_OPTION} -DDEVICE_MODE=ON -DKERNEL_MODE=ON -DPRODUCT=ascend -DPRODUCT_SIDE=device -DUSE_ALOG=0 -DCUSTOM_SIGN_SCRIPT=${CUSTOM_SIGN_SCRIPT} -DENABLE_SIGN=${ENABLE_SIGN} -DVERSION_INFO=${VERSION_INFO}"
    build_kernel
    cd .. & cd ${BUILD_DIR}
    CUSTOM_OPTION="${CURRENT_CUSTOM_OPTION} -DDEVICE_MODE=OFF -DPRODUCT=ascend -DPRODUCT_SIDE=host -DUSE_ALOG=1"
    build_package
    rmdir ${BUILD_DEVICE_DIR}
fi
