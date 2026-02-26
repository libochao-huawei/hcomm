# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HCOMM (Huawei Communication) is the foundational communication library for HCCL (Huawei Collective Communication Library). It provides communication domain and resource management capabilities for Huawei Ascend AI processors.

- **Language**: C++14
- **Build System**: CMake (>=3.16.0)
- **License**: CANN Open Software License Agreement Version 2.0
- **Target Hardware**: Huawei Ascend AI Processors (910B, 91093, 310P, A3 series)

## Architecture

HCOMM uses a layered decoupled design with two main planes:

1. **Control Plane (控制面)**: Topology query and communication resource management
   - Framework layer: communicator management, cluster maintenance
   - Platform layer: HCCP protocol stack, resource management, task dispatch

2. **Data Plane (数据面)**: Local operations, inter-operator synchronization, communication operations
   - Algorithm layer: collective communication algorithm implementations
   - Operator execution: AllReduce, AllGather, Broadcast, etc.

### Key Libraries

- `hcomm` - Main host-side shared library
- `hccl_plf` - Platform layer shared library
- `hccl_alg` - Algorithm layer library
- `ccl_kernel` - AI CPU kernel library
- `hccp_service.bin` - Device-side service binary

## Build Commands

### Prerequisites

Install third-party dependencies and set up the environment:

```bash
# Install third-party libraries (googletest, nlohmann_json)
bash build_third_party.sh --output_path=./output/third_party

# Source CANN environment (required for all builds)
source /usr/local/Ascend/cann/set_env.sh  # or $HOME/Ascend/cann/set_env.sh
```

### Building

```bash
# Build host package only
bash build.sh --pkg

# Full build (host + device components)
bash build.sh --pkg --full

# Build with custom CANN path
bash build.sh --pkg -p /path/to/cann/toolkit

# Build with specific options
bash build.sh --pkg -j16 --build-type=Release
```

Build output is placed in `./build_out/` with the installable package `cann-hcomm_<version>_linux-<arch>.run`.

### Build Options

| Option | Description |
|--------|-------------|
| `--pkg` | Build host package |
| `--full` | Full build including device components |
| `-p <path>` | Specify CANN package path |
| `-j<N>` | Set parallel build jobs |
| `--build-type=Release\|Debug` | Build type (default: Release) |
| `--asan` | Enable AddressSanitizer |
| `--cov` | Enable code coverage |
| `--aicpu` | Build only ccl_kernel.so |
| `--noclean` | Preserve build directory |

## Testing

### Unit Tests (UT)

```bash
# Build and run all unit tests
bash build.sh --ut

# With coverage
bash build.sh --ut --cov
```

UT executables are in `./build/test/`. Run individual tests:

```bash
./build/test/hccl_utest_framework_op_base_api --gtest_filter=HcclCommInitRootInfoTest.*
```

### System Tests (ST)

```bash
# Run all ST tests
bash build.sh --st

# Run specific test suites
bash build.sh --open_hccl_test
bash build.sh --executor_hccl_test
bash build.sh --executor_reduce_hccl_test
bash build.sh --executor_pipeline_hccl_test
```

Individual test binaries:

```bash
./build/test/st/algorithm/testcase/testcase/open_hccl_test
./build/test/st/algorithm/testcase/executor_testcase_generalization/executor_hccl_test
./build/test/st/algorithm/testcase/executor_reduce_testcase_generalization/executor_reduce_hccl_test
./build/test/st/algorithm/testcase/executor_alltoall_A3_pipeline_testcase/executor_pipeline_hccl_test
```

## Directory Structure

```
src/
├── algorithm/           # Communication algorithm implementations
│   ├── base/            # Algorithm templates (Ring, Mesh, Pipeline patterns)
│   └── impl/            # Algorithm implementations (coll_executor, operators)
├── framework/           # Control plane framework
│   ├── cluster_maintenance/   # Snapshot, heartbeat, operator retry
│   ├── communicator/    # Communication domain management
│   ├── device/          # AI CPU implementation (aicpu_kfc)
│   └── hcom/            # HCOM interface implementation
├── platform/            # Communication platform (data plane)
│   ├── comm_primitive/  # Communication primitives
│   ├── hccp/            # HCCP collective communication protocol
│   ├── resource/        # Transport, notify, stream resources
│   └── task/            # Task dispatch management
├── common/              # Shared utilities
├── hccd/                # HCCD (dataflow) implementation
└── pub_inc/             # Public internal headers

include/hccl/            # External API headers
├── hccl_types.h         # Type definitions (HcclResult, HcclDataType, etc.)
├── hccl_comm.h          # Communicator management
└── hccl_res.h           # Resource management

test/
├── ut/                  # Unit tests (Google Test)
├── st/                  # System tests
└── legacy/              # Legacy compatibility tests
```

## Key CMake Variables

| Variable | Description |
|----------|-------------|
| `BUILD_OPEN_PROJECT` | Build open source project (default: ON) |
| `DEVICE_MODE` | Build for device side |
| `KERNEL_MODE` | Build kernel components |
| `PRODUCT` | Target product (ascend, ascend910B) |
| `PRODUCT_SIDE` | host or device |
| `ENABLE_TEST` | Enable test builds |
| `ENABLE_UT` | Enable unit tests |
| `ENABLE_ST` | Enable system tests |

## Error Handling

HCCL uses `HcclResult` return codes defined in `include/hccl/hccl_types.h`:

- `HCCL_SUCCESS = 0` - Success
- `HCCL_E_PARA = 1` - Parameter error
- `HCCL_E_MEMORY = 3` - Memory error
- `HCCL_E_INTERNAL = 4` - Internal error
- `HCCL_E_NOT_SUPPORT = 5` - Not supported
- `HCCL_E_TIMEOUT = 9` - Timeout

## Code Style Notes

- C++14 standard
- Use Chinese for business logic comments, English for API documentation
- Error handling: Return `HcclResult` codes
- All warnings treated as errors (`-Werror`)
