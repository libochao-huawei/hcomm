#!/bin/bash
# Copyright (c) 2024 Huawei Technologies Co., Ltd.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ======================================================================================================================

set -e

BASEPATH=$(cd "$(dirname $0)/.."; pwd)

# print usage message
usage() {
  echo "Usage:"
  echo "sh run_test.sh [-u | --ut] [-s | --st]"
  echo "               [-c | --cov] [-j<N>] [-h | --help] [-v | --verbose]"
  echo "               [--ascend_install_path=<PATH>] [--ascend_3rd_lib_path=<PATH>]"
  echo ""
  echo "Options:"
  echo "    -u, --ut       Build all ut"
  echo "        =ge               Build all ge ut"
  echo "            =ge_common    Build ge common ut"
  echo "            =rt           Build ge runtime ut"
  echo "            =python       Build ge python ut"
  echo "            =parser       Build ge parser ut"
  echo "            =dflow        Build ge dflow ut"
  echo "        =engines          Build all engines ut"
  echo "            =fe           Build fusion engine ut"
  echo "            =dvpp         Build dvpp engine ut"
  echo "            =aicpu        Build aicpu engine ut"
  echo "            =ffts         Build ffts engine ut"
  echo "            =hcce         Build hcce engine ut"
  echo "        =executor_c       Build executor_c ut"
  echo "        =autofuse         Build autofuse ut"
  echo "    -s, --st       Build all st"
  echo "        =ge               Build all ge st"
  echo "            =ge_common    Build ge common st"
  echo "            =rt           Build ge runtime st"
  echo "            =hetero       Build ge hetero st"
  echo "            =python       Build ge python st"
  echo "            =parser       Build ge parser st"
  echo "            =dflow        Build ge dflow st"
  echo "        =engines          Build all engines st"
  echo "            =fe           Build fusion engine st"
  echo "            =dvpp         Build dvpp engine st"
  echo "            =aicpu        Build aicpu engine st"
  echo "            =ffts         Build ffts engine st"
  echo "            =hcce         Build hcce engine st"
  echo "        =executor_c       Build executor_c st"
  echo "        =autofuse         Build autofuse st"
  echo "    -h, --help     Print usage"
  echo "    -c, --cov      Build ut/st with coverage tag"
  echo "                   Please ensure that the environment has correctly installed lcov, gcov, and genhtml."
  echo "                   and the version matched gcc/g++."
  echo "    -v, --verbose  Display build command"
  echo "    -j<N>          Set the number of threads used for building Parser, default 8"
  echo "        --ascend_install_path=<PATH>"
  echo "                   Set ascend package install path, default /usr/local/Ascend/ascend-toolkit/latest"
  echo "        --ascend_3rd_lib_path=<PATH>"
  echo "                   Set ascend third_party package install path, default ./output/third_party"
  echo ""
}


# $1: ENABLE_UT/ENABLE_ST
# $2: ENABLE_ST/ENABLE_UT
# $3: ENABLE_GE
# $4: ENABLE_ENGINES
# $5: input value of ut or st
check_on() {
  if [ "X$1" = "Xon" ]; then
    usage;
    exit 1
  elif [ "X$2" = "Xon" ]; then
    if [ "X$3" != "Xon" ] || [ "X$4" != "Xon" ] || [ -n "$5" ]; then
      usage
      exit 1
    fi
  fi
}


# parse and set options
checkopts() {
  VERBOSE=""
  THREAD_NUM=16
  COVERAGE=""

  ENABLE_UT="off"
  ENABLE_ST="off"

  ENABLE_GE="off"
  ENABLE_GE_COMMON="off"
  ENABLE_RT="off"
  ENABLE_HETERO="off"
  ENABLE_PYTHON="off"
  ENABLE_PARSER="off"
  ENABLE_DFLOW="off"
  ENABLE_ENGINES="off"
  ENABLE_FE="off"
  ENABLE_FFTS="off"
  ENABLE_HCCE="off"
  ENABLE_AICPU="off"
  ENABLE_DVPP="off"
  ENABLE_ST_WHOLE_PROCESS="off"
  ENABLE_GE_C="off"
  ENABLE_GE_AUTOFUSE="off"

  ENABLE_GE_BENCHMARK="off"

  if [ -n "$ASCEND_INSTALL_PATH" ]; then
    ASCEND_INSTALL_PATH="$ASCEND_INSTALL_PATH"
  else
    ASCEND_INSTALL_PATH="/usr/local/Ascend/ascend-toolkit/latest"
  fi

  ASCEND_3RD_LIB_PATH="$BASEPATH/output/third_party"
  BUILD_METADEF="off"

  parsed_args=$(getopt -a -o u::s::cj:hv -l ut::,st::,process_st::,cov,help,verbose,ascend_install_path:,ascend_3rd_lib_path: -- "$@") || {
    usage
    exit 1
  }

  eval set -- "$parsed_args"

  while true; do
    case "$1" in
      -u | --ut)
        check_on "$ENABLE_UT" "$ENABLE_ST" "$ENABLE_GE" "$ENABLE_ENGINES" "$2"
        ENABLE_UT="on"
        case "$2" in
          "")
            ENABLE_GE="on"
            ENABLE_GE_COMMON="on"
            ENABLE_RT="on"
            ENABLE_PYTHON="on"
            ENABLE_PARSER="on"
            ENABLE_DFLOW="on"
            ENABLE_ENGINES="on"
            ENABLE_FE="on"
            ENABLE_DVPP="on"
            ENABLE_AICPU="on"
            ENABLE_FFTS="on"
            ENABLE_HCCE="on"
            shift 2
            ;;
          "ge")
            ENABLE_GE_COMMON="on"
            ENABLE_RT="on"
            ENABLE_PYTHON="on"
            ENABLE_GE="on"
            ENABLE_PARSER="on"
            ENABLE_DFLOW="on"
            shift 2
            ;;
          "ge_common")
            ENABLE_GE_COMMON="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "rt")
            ENABLE_RT="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "python")
            ENABLE_PYTHON="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "parser")
            ENABLE_PARSER="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "dflow")
            ENABLE_DFLOW="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "engines")
            ENABLE_ENGINES="on"
            ENABLE_FE="on"
            ENABLE_DVPP="on"
            ENABLE_AICPU="on"
            ENABLE_FFTS="on"
            ENABLE_HCCE="on"
            shift 2
            ;;
          "fe")
            ENABLE_ENGINES="on"
            ENABLE_FE="on"
            BUILD_METADEF="on"
            shift 2
            ;;
          "dvpp")
            ENABLE_ENGINES="on"
            ENABLE_DVPP="on"
            shift 2
            ;;
          "aicpu")
            ENABLE_ENGINES="on"
            ENABLE_AICPU="on"
            shift 2
            ;;
          "ffts")
            ENABLE_ENGINES="on"
            ENABLE_FFTS="on"
            shift 2
            ;;
          "hcce")
            ENABLE_ENGINES="on"
            ENABLE_HCCE="on"
            shift 2
            ;;
          "executor_c")
            ENABLE_GE_C="on"
            shift 2
            ;;
          "autofuse")
            ENABLE_GE_AUTOFUSE="on"
            shift 2
            ;;
          *)
            usage
            exit 1
        esac
        ;;
      -s | --st)
        check_on "$ENABLE_ST" "$ENABLE_UT" "$ENABLE_GE" "$ENABLE_ENGINES" "$2"
        ENABLE_ST="on"
        case "$2" in
          "")
            ENABLE_GE="on"
            ENABLE_GE_COMMON="on"
            ENABLE_RT="on"
            ENABLE_HETERO="on"
            ENABLE_PYTHON="on"
            ENABLE_PARSER="on"
            ENABLE_DFLOW="on"
            ENABLE_ENGINES="on"
            ENABLE_FE="on"
            ENABLE_DVPP="on"
            ENABLE_AICPU="on"
            ENABLE_FFTS="on"
            ENABLE_HCCE="on"
            shift 2
            ;;
          "ge")
            ENABLE_GE_COMMON="on"
            ENABLE_RT="on"
            ENABLE_HETERO="on"
            ENABLE_PYTHON="on"
            ENABLE_PARSER="on"
            ENABLE_DFLOW="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "ge_common")
            ENABLE_GE_COMMON="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "rt")
            ENABLE_RT="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "hetero")
            ENABLE_HETERO="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "python")
            ENABLE_PYTHON="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "parser")
            ENABLE_PARSER="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "dflow")
            ENABLE_DFLOW="on"
            ENABLE_GE="on"
            shift 2
            ;;
          "engines")
            ENABLE_ENGINES="on"
            ENABLE_FE="on"
            ENABLE_DVPP="on"
            ENABLE_AICPU="on"
            ENABLE_FFTS="on"
            ENABLE_HCCE="on"
            shift 2
            ;;
          "fe")
            ENABLE_ENGINES="on"
            ENABLE_FE="on"
            BUILD_METADEF="on"
            shift 2
            ;;
          "dvpp")
            ENABLE_ENGINES="on"
            ENABLE_DVPP="on"
            shift 2
            ;;
          "aicpu")
            ENABLE_ENGINES="on"
            ENABLE_AICPU="on"
            shift 2
            ;;
          "ffts")
            ENABLE_ENGINES="on"
            ENABLE_FFTS="on"
            shift 2
            ;;
          "hcce")
            ENABLE_ENGINES="on"
            ENABLE_HCCE="on"
            shift 2
            ;;
          "executor_c")
            ENABLE_GE_C="on"
            shift 2
            ;;
          "autofuse")
            ENABLE_GE_AUTOFUSE="on"
            shift 2
            ;;
          *)
            usage
            exit 1
        esac
        ;;
      --process_st)
        ENABLE_ST="on"
        ENABLE_ENGINES="on"
        ENABLE_FE="on"
        ENABLE_ST_WHOLE_PROCESS="on"
        BUILD_METADEF="on"
        shift 2
        ;;
      -c | --cov)
        COVERAGE="-c"
        shift
        ;;
      -h | --help)
        usage
        exit 1
        ;;
      -j)
        THREAD_NUM=$2
        shift 2
        ;;
      -v | --verbose)
        VERBOSE="-v"
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

main() {
  cd "${BASEPATH}"
  checkopts "$@"

  if [ "X$ENABLE_UT" != "Xon" ] && [ "X$ENABLE_ST" != "Xon" ]; then
    ENABLE_UT="on"
    ENABLE_ST="on"
    ENABLE_GE="on"
    ENABLE_GE_COMMON="on"
    ENABLE_RT="on"
    ENABLE_HETERO="on"
    ENABLE_PYTHON="on"
    ENABLE_PARSER="on"
    ENABLE_DFLOW="on"
    ENABLE_ENGINES="on"
    ENABLE_FE="on"
    ENABLE_DVPP="on"
    ENABLE_AICPU="on"
    ENABLE_FFTS="on"
    ENABLE_HCCE="on"
  fi

  export BUILD_METADEF=${BUILD_METADEF}
  export ASCEND_INSTALL_PATH=${ASCEND_INSTALL_PATH}
  export ASCEND_3RD_LIB_PATH=${ASCEND_3RD_LIB_PATH}

  # module ge
  if [ "X$ENABLE_GE" = "Xon" ]; then
    # ge ut
    if [ "X$ENABLE_UT" == "Xon" ]; then
      if [ "X$ENABLE_GE_COMMON" = "Xon" ] && [ "X$ENABLE_RT" = "Xon" ] && [ "X$ENABLE_PYTHON" = "Xon" ] && [ "X$ENABLE_PARSER" = "Xon" ] && [ "X$ENABLE_DFLOW" = "Xon" ]; then
        bash scripts/build_fwk.sh -t -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_GE_COMMON" = "Xon" ]; then
        bash scripts/build_fwk.sh -T -j $THREAD_NUM $VERBOSE $COVERAGE
        bash scripts/build_fwk.sh -d -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_RT" = "Xon" ]; then
        bash scripts/build_fwk.sh -L -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_PYTHON" = "Xon" ]; then
        bash scripts/build_fwk.sh -l -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_PARSER" = "Xon" ]; then
        bash scripts/build_fwk.sh -m -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_DFLOW" = "Xon" ]; then
        bash scripts/build_fwk.sh -o -j $THREAD_NUM $VERBOSE $COVERAGE
      else
        echo "unknown ut type."
      fi
    fi

    # ge st
    if [ "X$ENABLE_ST" = "Xon" ]; then
      if [ "X$ENABLE_GE_COMMON" = "Xon" ] && [ "X$ENABLE_RT" = "Xon" ] && [ "X$ENABLE_HETERO" = "Xon" ] && [ "X$ENABLE_PYTHON" == "Xon" ] && [ "X$ENABLE_PARSER" == "Xon" ] && [ "X$ENABLE_DFLOW" == "Xon" ]; then
        bash scripts/build_fwk.sh -s -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_GE_COMMON" = "Xon" ]; then
        bash scripts/build_fwk.sh -O -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_RT" = "Xon" ]; then
        bash scripts/build_fwk.sh -R -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_HETERO" = "Xon" ]; then
        bash scripts/build_fwk.sh -K -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_PYTHON" = "Xon" ]; then
        bash scripts/build_fwk.sh -P -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_PARSER" = "Xon" ]; then
        bash scripts/build_fwk.sh -n -j $THREAD_NUM $VERBOSE $COVERAGE
      elif [ "X$ENABLE_DFLOW" = "Xon" ]; then
        bash scripts/build_fwk.sh -D -j $THREAD_NUM $VERBOSE $COVERAGE
      else
        echo "unknown type st."
      fi
    fi
  fi

  # module fe
  if [ "X$ENABLE_ENGINES" = "Xon" ]; then
    # engines ut
    if [ "X$ENABLE_UT" == "Xon" ]; then
      if [ "X$ENABLE_FE" = "Xon" ]; then
        bash scripts/build.sh -u -n -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
      if [ "X$ENABLE_DVPP" = "Xon" ]; then
        bash scripts/build.sh -k -u -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
      if [ "X$ENABLE_AICPU" = "Xon"  ]; then
        bash scripts/build.sh -a -u -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
      if [ "X$ENABLE_FFTS" = "Xon" ]; then
        bash scripts/build.sh -f -u -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
      if [ "X$ENABLE_HCCE" = "Xon" ]; then
        bash scripts/build.sh -e -u -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
    fi

    # engines st
    if [ "X$ENABLE_ST" = "Xon" ]; then
      if [ "X$ENABLE_FE" = "Xon" ]; then
        bash scripts/build.sh -s -n -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
      if [ "X$ENABLE_DVPP" = "Xon" ]; then
        bash scripts/build.sh -k -s -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
      if [ "X$ENABLE_AICPU" = "Xon" ]; then
        bash scripts/build.sh -a -s -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
      if [ "X$ENABLE_FFTS" = "Xon" ]; then
        bash scripts/build.sh -f -s -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
      if [ "X$ENABLE_HCCE" = "Xon" ]; then
        bash scripts/build.sh -e -s -j $THREAD_NUM $VERBOSE $COVERAGE
      fi

      # fe process st
      if [ "X$ENABLE_ST_WHOLE_PROCESS" = "Xon" ]; then
        bash scripts/build.sh -w -n -j $THREAD_NUM $VERBOSE $COVERAGE
      fi
    fi
  fi

  # module executor_c
  if [ "X$ENABLE_GE_C" == "Xon" ]; then
    # executor_c ut
    if [ "X$ENABLE_UT" = "Xon" ]; then
      bash scripts/build_executor_c.sh -u -j $THREAD_NUM $VERBOSE $COVERAGE
    fi

    # executor_c st
    if [ "X$ENABLE_ST" = "Xon" ]; then
      bash scripts/build_executor_c.sh -s -j $THREAD_NUM $VERBOSE $COVERAGE
    fi
  fi

  # module autofuse
  if [ "X$ENABLE_GE_AUTOFUSE" == "Xon" ]; then
    # executor_c ut
    if [ "X$ENABLE_UT" = "Xon" ]; then
      bash scripts/test/run_autofuse_test.sh -u -j $THREAD_NUM $VERBOSE $COVERAGE
    fi

    # executor_c st
    if [ "X$ENABLE_ST" = "Xon" ]; then
      bash scripts/test/run_autofuse_test.sh -s -j $THREAD_NUM $VERBOSE $COVERAGE
    fi
  fi
}

main "$@"
