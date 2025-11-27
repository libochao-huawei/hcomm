#!/bin/bash
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ======================================================================================================================

set -e
BASEPATH=$(cd "$(dirname $0)/.."; pwd)
OUTPUT_PATH="${BASEPATH}/output"
BUILD_RELATIVE_PATH="build_ut"
BUILD_PATH="${BASEPATH}/${BUILD_RELATIVE_PATH}"

# print usage message
usage() {
  echo "Usage:"
  echo "sh run_test.sh [-h | --help] [-v | --verbose] [-j<N>]"
  echo "               [--ascend_install_path=<PATH>] [--ascend_3rd_lib_path=<PATH>]"
  echo "               [-u | --ut] [-c | --cov]"
  echo ""
  echo "Options:"
  echo "    -h, --help     Print usage"
  echo "    -v, --verbose  Display build command"
  echo "    -j<N>          Set the number of threads used for building, default is 8"
  echo "        --ascend_install_path=<PATH>"
  echo "                   Set ascend package install path, default /usr/local/Ascend/ascend-toolkit/latest"
  echo "        --ascend_3rd_lib_path=<PATH>"
  echo "                   Set ascend third_party package install path, default ./output/third_party"
  echo "    -u, --ut       Build and execute ut"
  echo "    -c, --cov      Build ut with coverage tag"
  echo "                   Please ensure that the environment has correctly installed lcov, gcov, and genhtml."
  echo "                   and the version matched gcc/g++."
  echo ""
}

# parse and set options
checkopts() {
  VERBOSE=""
  THREAD_NUM=8
  ENABLE_UT="on"
  ENABLE_COV="off"
  ASCEND_3RD_LIB_PATH="$BASEPATH/output/third_party"
  CMAKE_BUILD_TYPE="Release"

  if [ -z "$ASCEND_INSTALL_PATH" ]; then
    ASCEND_INSTALL_PATH="/usr/local/Ascend/ascend-toolkit/latest"
  fi

  # Process the options
  parsed_args=$(getopt -a -o j:hvuc -l help,verbose,ascend_install_path:,ascend_3rd_lib_path:,ut,cov -- "$@") || {
    usage
    exit 1
  }

  eval set -- "$parsed_args"

  while true; do
    case "$1" in
      -h | --help)
        usage
        exit 0
        ;;
      -j)
        THREAD_NUM="$2"
        shift 2
        ;;
      -v | --verbose)
        VERBOSE="VERBOSE=1"
        shift
        ;;
      --ascend_install_path)
        ASCEND_INSTALL_PATH="$(realpath $2)"
        shift 2
        ;;
      --ascend_3rd_lib_path)
        ASCEND_3RD_LIB_PATH="$(realpath $2)"
        shift 2
        ;;
      -u | --ut)
        ENABLE_UT="on"
        shift
        ;;
      -c | --cov)
        ENABLE_UT="on"
        ENABLE_COV="on"
        shift
        ;;
      --)
        shift
        break
        ;;
      *)
        echo "Undefined option: $1"
        usage
        exit 1
        ;;
    esac
  done
}

mk_dir() {
  local create_dir="$1"  # the target to make
  mkdir -pv "${create_dir}"
  echo "created ${create_dir}"
}

# create build path
build_hccl() {
  echo "create build directory and build";
  mk_dir "${BUILD_PATH}"
  local report_dir="${OUTPUT_PATH}/report/ut" && mk_dir "${report_dir}"
  cd "${BUILD_PATH}"

  local LLT_KILL_TIME=1200
  CMAKE_ARGS="-DENABLE_OPEN_SRC=True \
              -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
              -DCMAKE_INSTALL_PREFIX=${OUTPUT_PATH} \
              -DASCEND_INSTALL_PATH=${ASCEND_INSTALL_PATH} \
              -DASCEND_3RD_LIB_PATH=${ASCEND_3RD_LIB_PATH} \
              -DCANN_3RD_LIB_PATH=${ASCEND_3RD_LIB_PATH} \
              -DENABLE_COV=${ENABLE_COV} \
              -DENABLE_UT=${ENABLE_UT} \
              -DOUTPUT_PATH=${OUTPUT_PATH} \
              -DLLT_KILL_TIME=${LLT_KILL_TIME} \
              -DENABLE_TEST=ON"

  echo "CMAKE_ARGS=${CMAKE_ARGS}"
  cmake ${CMAKE_ARGS} ..
  if [ $? -ne 0 ]; then
    echo "execute command: cmake ${CMAKE_ARGS} .. failed."
    return 1
  fi


  make ${VERBOSE} hccl_ut_prepare_env hccl_utest_platform_aicpu -j${THREAD_NUM}

  run_ret=${PIPESTATUS[0]}
  echo "exit code: ${run_ret}"
  if [ "${run_ret}" -eq 137 ]
  then
      echo "timeout: execute more than ${LLT_KILL_TIME}s killed"
      exit 1
  fi
  if [ $? -ne 0 ]; then
    echo "execute command: make ${VERBOSE} -j${THREAD_NUM} failed."
    return 1
  fi
  # hccl_utest_misc

  echo "build success!"
}

run_hccl_ut() {
  if [[ "X$ENABLE_UT" = "Xon" || "X$ENABLE_COV" = "Xon" ]]; then
    # cp ${BUILD_PATH}/test/llt/ut/prepare_ut_env/hccl_ut_prepare_env ${OUTPUT_PATH}
    # cp ${BUILD_PATH}/test/llt/ut/prepare_ut_env/hccl_utest_misc ${OUTPUT_PATH}

    # ORIGINAL_LD_PRELOAD="$LD_PRELOAD"
    # LIBASAN_PATH=$(gcc -print-file-name=libasan.so)
    # if [ -f "$LIBASAN_PATH" ]; then
    #   export LD_PRELOAD="$ORIGINAL_LD_PRELOAD:$LIBASAN_PATH"
    #   echo "preload libasan from $LIBASAN_PATH"
    # else
    #   echo "libasan not found for the current gcc version."
    # fi

    # local report_dir="${OUTPUT_PATH}/report/ut" && mk_dir "${report_dir}"
    # RUN_TEST_CASE="${OUTPUT_PATH}/hccl_ut_prepare_env --gtest_output=xml:${report_dir}/hccl_ut_prepare_env.xml" && ${RUN_TEST_CASE}
    # RUN_TEST_CASE="${OUTPUT_PATH}/hccl_utest_misc --gtest_output=xml:${report_dir}/hccl_utest_misc.xml" && ${RUN_TEST_CASE}
    # if [ -n "$ORIGINAL_LD_PRELOAD" ]; then
    #   export LD_PRLOAD="$ORIGINAL_LD_PRELOAD"
    # else
    #   unset LD_PRELOAD
    # fi

    # if [[ "$?" -ne 0 ]]; then
    #   echo "!!! UT FAILED, PLEASE CHECK YOUR CHANGES !!!"
    #   echo -e "\033[31m${RUN_TEST_CASE}\033[0m"
    #   exit 1;
    # fi

    echo "Generated coverage statistics, please wait..."
    cd ${BASEPATH}
    rm -rf ${BASEPATH}/cov
    mkdir -p ${BASEPATH}/cov
    lcov -c -d ${BUILD_RELATIVE_PATH}/test/llt/ut/ -o cov/tmp.info
    lcov -c -d ${BUILD_RELATIVE_PATH}/test/llt/ut/prepare_ut_env/ -o cov/tmp.info
    LCOV_COMMAND="lcov -r cov/tmp.info ${BASEPATH}src/* -o cov/coverage.info" && ${LCOV_COMMAND}
    # lcov -r cov/tmp.info "/usr/*" "${OUTPUT_PATH}/*" "${BASEPATH}/test/*" "${ASCEND_INSTALL_PATH}/*" "${ASCEND_3RD_LIB_PATH}/*" -o cov/coverage.info

    cd ${BASEPATH}/cov
    genhtml coverage.info
  fi
}

main() {
  checkopts "$@"

  # build start
  echo "---------------- build start ----------------"
  g++ -v
  mk_dir ${OUTPUT_PATH}
  build_hccl
  if [[ "$?" -ne 0 ]]; then
    echo "build failed.";
    exit 1;
  fi
  echo "---------------- build finished ----------------"

  echo "---------------- output generated ----------------"
  run_hccl_ut
}

main "$@"
