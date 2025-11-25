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
OUTPUT_DIR=${CURRENT_DIR}/build_out
USER_ID=$(id -u)
CPU_NUM=$(($(cat /proc/cpuinfo | grep "^processor" | wc -l)*2))
JOB_NUM="-j${CPU_NUM}"
ASAN="false"
COV="false"
CUSTOM_OPTION="-DCMAKE_INSTALL_PREFIX=${OUTPUT_DIR}"
FULL_MODE="true"  # 新增变量，用于控制是否全量构建
KERNEL="false"  # 新增变量，用于控制是否只编译 ccl_kernel.so
DOWNLOAD_MOCKCPP="false"  # 新增变量，控制是否下载和编译 mockcpp
DO_NOT_CLEAN="false" # 是否清理
CANN_3RD_LIB_PATH="${CURRENT_DIR}/third_party"
CANN_UTILS_LIB_PATH="${CURRENT_DIR}/utils"
BUILD_AARCH="false"
CUSTOM_SIGN_SCRIPT="${CURRENT_DIR}/../vendor/hisi/build/scripts/sign_and_add_header.sh"
ENABLE_SIGN="true"

BUILD_FWK_HLT="false"
MOCK_FWK_HLT="0"

BUILD_CB_TEST="false"

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
    source $ASCEND_CANN_PACKAGE_PATH/bin/setenv.bash || echo "0"
}

function clean()
{
    if [ -n "${BUILD_DIR}" ];then
        rm -rf ${BUILD_DIR}
    fi

    if [ -z "${TEST}" ] && [ -z "${KERNEL}" ];then
        if [ -n "${OUTPUT_DIR}" ];then
            rm -rf ${OUTPUT_DIR}
        fi
    fi

    mkdir -p ${BUILD_DIR}
}

function download_mockcpp() {

    if [ -d "${MOCKCPP_DIR}/mockcpp" ];then
        rm -rf ${MOCKCPP_DIR}/mockcpp
        log "Info: delete ${MOCKCPP_DIR}/mockcpp"
    fi

    if [ -d "${MOCKCPP_BUILD_DIR}" ]; then
        log "Info: mockcpp already built, skipping download and compilation."
        return
    fi

    # 下载mockcpp
    log "Info: Downloading mockcpp..."

    cd ${MOCKCPP_DIR}

    git clone https://gitee.com/sinojelly/mockcpp.git || {
        log "ERROR: Failed to download mockcpp."
        log "ERROR: Please execute separately [git clone https://gitee.com/sinojelly/mockcpp.git]"
        exit 1
    }
}

function build_mockcpp() {

    cd ${MOCKCPP_BUILD_DIR}
    log "Info compiler mockcpp"

    cmake_config "-DCMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=0"
    build mockcpp

    chmod 775 src/libmockcpp.a
}

function cmake_config()
{
    local extra_option="$1"
    log "Info: cmake config ${CUSTOM_OPTION} ${extra_option} ."
    cmake ..  ${CUSTOM_OPTION} ${extra_option}
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
    TARGET_LIST="hccp_service.bin rs ccl_kernel_plf ccl_kernel_plf_a ccl_kernel"
    echo "TARGET_LIST=${TARGET_LIST}"
    PKG_TARGET_LIST="generate_device_hccp_package generate_device_aicpu_package"
    echo "PKG_TARGET_LIST=${PKG_TARGET_LIST}"
    SIGN_TARGET_LIST=""
    if [ "${ENABLE_SIGN}" == "true" ]; then
        SIGN_TARGET_LIST="sign_hcomm_device sign_aicpu_hcomm"
    fi
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
    SIGN_TARGET_LIST=""
    if [ "${ENABLE_SIGN}" == "true" ]; then
        SIGN_TARGET_LIST="sign_hcomm_hccd"
    fi
    echo "SIGN_TARGET_LIST=${SIGN_TARGET_LIST}"
    build ${TARGET_LIST} ${PKG_TARGET_LIST} ${SIGN_TARGET_LIST}
}

function build_cb_test_verify(){
    cd ${CURRENT_DIR}/examples/
    source ${ASCEND_CANN_PACKAGE_PATH}/bin/setenv.bash
    bash build.sh
}

function build_test() {
    cmake_config

    LIBRARY_DIR="${BUILD_DIR}/test:${ASCEND_HOME_PATH}/lib64:"
    # 每日构建sdk包安装路径
    if [ -d "${ASCEND_HOME_PATH}/opensdk" ];then
        LIBRARY_DIR="${LIBRARY_DIR}${ASCEND_HOME_PATH}/opensdk/opensdk/gtest_shared/lib64:"
    fi

    # 社区sdk包安装路径
    if [ -d "${ASCEND_HOME_PATH}/../../latest/opensdk" ];then
        LIBRARY_DIR="${LIBRARY_DIR}${ASCEND_HOME_PATH}/../../latest/opensdk/opensdk/gtest_shared/lib64:"
    fi

    GCC_MAJOR=`gcc -dumpversion | cut -d. -f1`
    if [ "${ASAN}" == "true" ];then
        ARCH=$(uname -m)
        if [[ $ARCH == "x86_64" || $ARCH == "i386" || $ARCH == "i686" ]]; then
            PRELOAD="/usr/lib/gcc/x86_64-linux-gnu/${GCC_MAJOR}/libasan.so:/usr/lib/gcc/x86_64-linux-gnu/${GCC_MAJOR}/libstdc++.so"
        elif [[ $ARCH == "aarch64" || $ARCH == "armv8l" || $ARCH == "armv7l" ]]; then
            PRELOAD="/usr/lib/gcc/aarch64-linux-gnu/${GCC_MAJOR}/libasan.so:/usr/lib/gcc/aarch64-linux-gnu/${GCC_MAJOR}/libstdc++.so"
        else
            echo "未知架构: $ARCH"
        fi
        echo "PRELOAD is ${PRELOAD}"
        ASAN_OPT="detect_leaks=0"
    fi

    if [ "${TEST_TASK_NAME}" == "open_hccl_test" ] || [ "$TEST" = "all" ];then
        build open_hccl_test
        export LD_LIBRARY_PATH=${LIBRARY_DIR}${LD_LIBRARY_PATH} && export LD_PRELOAD=${PRELOAD} && export ASAN_OPTIONS=${ASAN_OPT} \
        && ${BUILD_DIR}/test/open_hccl_test
    fi

    if [ "${TEST_TASK_NAME}" == "executor_hccl_test" ] || [ "$TEST" = "all" ];then
        build executor_hccl_test
        export LD_LIBRARY_PATH=${LIBRARY_DIR}${LD_LIBRARY_PATH} && export LD_PRELOAD=${PRELOAD} && export ASAN_OPTIONS=${ASAN_OPT} \
        && ${BUILD_DIR}/test/executor_hccl_test
    fi

    if [ "${TEST_TASK_NAME}" == "executor_reduce_hccl_test" ] || [ "$TEST" = "all" ];then
        build executor_reduce_hccl_test
        export LD_LIBRARY_PATH=${LIBRARY_DIR}${LD_LIBRARY_PATH} && export LD_PRELOAD=${PRELOAD} && export ASAN_OPTIONS=${ASAN_OPT} \
        && ${BUILD_DIR}/test/executor_reduce_hccl_test
    fi

    if [ "${TEST_TASK_NAME}" == "executor_pipeline_hccl_test" ] || [ "$TEST" = "all" ];then
        build executor_pipeline_hccl_test
        export LD_LIBRARY_PATH=${LIBRARY_DIR}${LD_LIBRARY_PATH} && export LD_PRELOAD=${PRELOAD} && export ASAN_OPTIONS=${ASAN_OPT} \
        && ${BUILD_DIR}/test/executor_pipeline_hccl_test
    fi

    if [ "${TEST_TASK_NAME}" == "device_testcase" ] || [ "$TEST" = "all" ];then
        build device_testcase
        export LD_LIBRARY_PATH=${LIBRARY_DIR}${LD_LIBRARY_PATH} && export LD_PRELOAD=${PRELOAD} && export ASAN_OPTIONS=${ASAN_OPT} \
        && ${BUILD_DIR}/test/device_testcase
    fi

    if [ "${TEST_TASK_NAME}" == "executor_aiv_hccl_test" ] || [ "$TEST" = "all" ];then
        build executor_aiv_hccl_test
        export LD_LIBRARY_PATH=${LIBRARY_DIR}${LD_LIBRARY_PATH} && export LD_PRELOAD=${PRELOAD} && export ASAN_OPTIONS=${ASAN_OPT} \
        && ${BUILD_DIR}/test/executor_aiv_hccl_test
    fi

}

function build_kernel() {
    cmake_config
    log "Info: build_kernel"
    build ccl_kernel_plf ccl_kernel_plf_a ccl_kernel aicpu_custom_json
}

while [[ $# -gt 0 ]]; do
    case $1 in
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
    -t|--test)
        TEST="all"
        DOWNLOAD_MOCKCPP="true"
        shift
        ;;
    --open_hccl_test)
        TEST="partial"
        DOWNLOAD_MOCKCPP="true"
        TEST_TASK_NAME="open_hccl_test"
        shift
        ;;
    --executor_hccl_test)
        TEST="partial"
        DOWNLOAD_MOCKCPP="true"
        TEST_TASK_NAME="executor_hccl_test"
        shift
        ;;
    --executor_reduce_hccl_test)
        TEST="partial"
        DOWNLOAD_MOCKCPP="true"
        TEST_TASK_NAME="executor_reduce_hccl_test"
        shift
        ;;
    --executor_pipeline_hccl_test)
        TEST="partial"
        DOWNLOAD_MOCKCPP="true"
        TEST_TASK_NAME="executor_pipeline_hccl_test"
        shift
        ;;
    --device_testcase)
        TEST="partial"
        DOWNLOAD_MOCKCPP="true"
        TEST_TASK_NAME="device_testcase"
        shift
        ;;
    --executor_aiv_hccl_test)
        TEST="partial"
        DOWNLOAD_MOCKCPP="true"
        TEST_TASK_NAME="executor_aiv_hccl_test"
        shift
        ;;
    --aicpu)  # 新增选项，用于只编译 ccl_kernel.so
        KERNEL="true"
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
    --sign_script=*)
        OPTARG=$1
        CUSTOM_SIGN_SCRIPT="$(realpath ${OPTARG#*=})"
        ENABLE_SIGN="true"
        shift
        ;;
    *)
        break
        ;;
    esac
done

if [ ! -f "$CUSTOM_SIGN_SCRIPT" ];then
    ENABLE_SIGN="false"
fi

if [ -n "${TEST}" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DENABLE_TEST=ON"
fi

if [ "${KERNEL}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DKERNEL_MODE=ON -DDEVICE_MODE=ON -DPRODUCT=ascend -DPRODUCT_SIDE=device"
fi

if [ "${BUILD_AARCH}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DAARCH_MODE=ON"
fi

if [ "${ASAN}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DENABLE_ASAN=true"
fi

if [ "${COV}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DENABLE_GCOV=true"
fi

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
# CUSTOM_OPTION="${CUSTOM_OPTION} -DCANN_3RD_LIB_PATH=${cann_3rd_lib_path}"
CUSTOM_OPTION="$CUSTOM_OPTION -DCANN_3RD_LIB_PATH=${CANN_3RD_LIB_PATH} -DCANN_UTILS_LIB_PATH=${CANN_UTILS_LIB_PATH}"

CUSTOM_OPTION="$CUSTOM_OPTION -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"

set_env

if [ "${DO_NOT_CLEAN}" = "false" ]; then
    clean
else
    mkdir -p "${BUILD_DIR}"
fi

if [ "${DOWNLOAD_MOCKCPP}" == "true" ]; then
    MOCKCPP_DIR="${CURRENT_DIR}/third_party"
    MOCKCPP_BUILD_DIR="${MOCKCPP_DIR}/mockcpp/build"
    # mockcpp编译过了之后就不编译了
    if [ -f "${MOCKCPP_BUILD_DIR}/src/libmockcpp.a" ];then
        log "Info: mockcpp is not compiler"
    else
        log "Info: begin compiler mockcpp"
        mkdir -p ${MOCKCPP_DIR}
        download_mockcpp

        if [ -d "${MOCKCPP_DIR}/mockcpp" ]; then
            mkdir -p ${MOCKCPP_BUILD_DIR}
            build_mockcpp
        else
            log "ERROR: The compilation directory does not exist."
        fi
    fi
fi

cd ${BUILD_DIR}

if [ -n "${TEST}" ];then
    build_test
elif [ "${KERNEL}" == "true" ]; then
    build_kernel
elif [ "${BUILD_FWK_HLT}" == "true" ]; then
    log "Info: Building fwk_test with MOCK_HCCL=${MOCK_FWK_HLT}"
    cmake ${CUSTOM_OPTION} -DMOCK_HCCL=${MOCK_FWK_HLT} ../test/hlt
    build hccl_fwk_test
    log "Info: fwk_test execution example: ${BUILD_DIR}/hccl_fwk_test --cluster_info test/hlt/ranktable.json --rank 0 --list"
    log "Info: fwk_test execution example: ${BUILD_DIR}/hccl_fwk_test --cluster_info test/hlt/ranktable.json --rank 0 --test allocthread"
elif [ "${BUILD_CB_TEST}" == "true" ]; then
    log "Info: Building cb_test_verify"
    build_cb_test_verify
    log "Info: Building cb_test_verify success"
elif [ "${FULL_MODE}" == "true" ]; then
    cd ..
    mkdir -p ${BUILD_DEVICE_DIR}
    cd ${BUILD_DEVICE_DIR}
    CURRENT_CUSTOM_OPTION="${CUSTOM_OPTION}"
    CUSTOM_OPTION="${CURRENT_CUSTOM_OPTION} -DFULL_MODE=ON -DDEVICE_MODE=ON -DKERNEL_MODE=ON -DPRODUCT=ascend910B -DPRODUCT_SIDE=device -DUSE_ALOG=0 -DCUSTOM_SIGN_SCRIPT=${CUSTOM_SIGN_SCRIPT} -DENABLE_SIGN=${ENABLE_SIGN}"
    build_device
    BUILD_HCCD_DIR="${CURRENT_DIR}/build_hccd"
    mkdir -p ${BUILD_HCCD_DIR}
    cd ${BUILD_HCCD_DIR}
    CUSTOM_OPTION="${CURRENT_CUSTOM_OPTION} -DDEVICE_MODE=ON -DPRODUCT=ascend -DPRODUCT_SIDE=device -DUSE_ALOG=1 -DCUSTOM_SIGN_SCRIPT=${CUSTOM_SIGN_SCRIPT} -DENABLE_SIGN=${ENABLE_SIGN}"
    build_hccd
    cd .. & cd ${BUILD_DIR}
    CUSTOM_OPTION="${CURRENT_CUSTOM_OPTION} -DDEVICE_MODE=OFF -DPRODUCT=ascend -DPRODUCT_SIDE=host -DUSE_ALOG=1"
    build_package
    rm -rf ${BUILD_DEVICE_DIR} ${BUILD_HCCD_DIR}
else
    build_package
fi