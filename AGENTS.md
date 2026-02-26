# HCOMM (Huawei Communication) - AI Agent Guide

## Project Overview

HCOMM (Huawei Communication) is the foundational communication library for HCCL (Huawei Collective Communication Library). It provides communication domain and resource management capabilities for Ascend AI processors.

### Key Characteristics

- **Technology Stack**: C++14, CMake (>=3.16.0), Python 3.7.x-3.11.4
- **License**: CANN Open Software License Agreement Version 2.0
- **Version**: 9.0.0
- **Platform**: Linux (x86_64, aarch64)
- **Target Hardware**: Huawei Ascend AI Processors (910B, 91093, 310P, A3 series, etc.)

### Architecture

HCOMM adopts a layered decoupled design with two main planes:

1. **Control Plane (控制面)**: Provides topology query and communication resource management
2. **Data Plane (数据面)**: Provides local operations, inter-operator synchronization, communication operations for data transfer and computation

### Key Features

- Multiple communication engines support on Ascend devices
- Multiple communication protocols: PCIe, HCCS, RDMA, RoCE
- Decoupled communication platform and operator development
- Support for various collective communication algorithms (Ring, Mesh, Pipeline, etc.)

## Project Structure

```
├── src/                              # Source code directory
│   ├── algorithm/                    # Communication algorithm implementation
│   │   ├── base/                     # Algorithm templates and bases
│   │   ├── impl/                     # Algorithm implementations
│   │   └── pub_inc/                  # Algorithm public headers
│   ├── framework/                    # Communication framework
│   │   ├── cluster_maintenance/      # Cluster maintenance (snapshot, heartbeat, retry)
│   │   ├── common/                   # Common utilities
│   │   ├── communicator/             # Communication domain management
│   │   ├── device/                   # AI CPU implementation
│   │   ├── hcom/                     # HCOM interface implementation
│   │   ├── nslbdp/                   # Network load balancing data plane
│   │   └── op_base/                  # Operator base interfaces
│   ├── platform/                     # Communication platform
│   │   ├── comm_primitive/           # Communication primitives
│   │   ├── common/                   # Platform common logic
│   │   ├── debug/                    # Debug and profiling
│   │   ├── hccp/                     # HCCP collective communication protocol
│   │   ├── ping_mesh/                # Network probing
│   │   ├── resource/                 # Resource management
│   │   └── task/                     # Task dispatch management
│   ├── hccd/                         # HCCD (dataflow) implementation
│   ├── legacy/                       # Legacy code compatibility
│   └── pub_inc/                      # Public internal headers
├── include/                          # External API headers
│   └── hccl/                         # HCCL public APIs
├── pkg_inc/                          # Package internal headers
├── python/                           # Python bindings
├── test/                             # Test code
│   ├── ut/                           # Unit tests
│   ├── st/                           # System tests
│   └── legacy/                       # Legacy tests
├── examples/                         # Example code
├── docs/                             # Documentation
├── cmake/                            # CMake configuration files
└── build.sh                          # Main build script
```

## Build System

### Prerequisites

- Python: 3.7.x to 3.11.4
- pip >= 20.3.0
- setuptools >= 45.0.0
- wheel >= 0.34.0
- gcc >= 7.3.0
- cmake >= 3.16.0
- ccache (recommended)
- nlohmann_json
- googletest (for UT, version release-1.14.0 recommended)

### Third-Party Dependencies

Install third-party libraries using:

```bash
bash build_third_party.sh --output_path=${THIRD_LIB_PATH}
```

Default output: `./output/third_party`

### CANN Toolkit Installation

HCOMM requires CANN Toolkit to be installed. Set environment:

```bash
# Default path for root user
source /usr/local/Ascend/cann/set_env.sh

# Default path for non-root user
source $HOME/Ascend/cann/set_env.sh
```

### Build Commands

#### Host Package Only

```bash
bash build.sh --pkg
```

#### Full Build (Host + Device)

```bash
bash build.sh --pkg --full
```

#### Build with Custom CANN Path

```bash
bash build.sh --pkg -p /path/to/cann/toolkit
```

#### Build Options

| Option | Description |
|--------|-------------|
| `--pkg` | Build host package |
| `--full` | Full build including device components |
| `-p <path>` | Specify CANN package path |
| `-j<N>` | Set parallel build jobs (default: CPU count) |
| `--build-type=Release\|Debug` | Build type (default: Release) |
| `--asan` | Enable AddressSanitizer |
| `--cov` | Enable code coverage |
| `--ut` or `-u` | Enable and run unit tests |
| `--st` or `-s` | Enable and run system tests |
| `--aicpu` | Build only ccl_kernel.so |
| `--noclean` | Do not clean build directory |
| `--enable-sign` | Enable package signing |

### Output

Build artifacts are placed in `./build_out/`:
- `cann-hcomm_<version>_linux-<arch>.run` - Installable package
- `hcomm/lib64/` - Libraries
- `hcomm/include/` - Headers

## Testing

### Unit Tests (UT)

```bash
bash build.sh --ut
```

Or with coverage:

```bash
bash build.sh --ut --cov
```

### System Tests (ST)

```bash
# Run all ST tests
bash build.sh --st

# Run specific tests
bash build.sh --open_hccl_test
bash build.sh --executor_hccl_test
bash build.sh --executor_reduce_hccl_test
bash build.sh --executor_pipeline_hccl_test
```

### Test Directory Structure

```
test/
├── ut/                 # Unit tests (Google Test framework)
├── st/                 # System tests
│   └── algorithm/      # Algorithm ST tests
└── legacy/             # Legacy compatibility tests
    ├── ut/
    └── st/
```

## Key APIs

HCOMM provides standard collective communication APIs in `include/hccl/`:

- `hccl_types.h` - Type definitions (HcclResult, HcclDataType, HcclReduceOp, etc.)
- `hccl_comm.h` - Communicator management
- `hccl_res.h` - Resource management
- `hccl_rank_graph.h` - Rank graph operations
- `hcomm_primitives.h` - Communication primitives

### Supported Collective Operations

- AllReduce
- AllGather / AllGatherV
- Reduce / ReduceScatter / ReduceScatterV
- Broadcast
- Scatter / Gather
- AllToAll / AllToAllV / AllToAllVC
- Send / Receive / BatchSendRecv

## Code Style Guidelines

1. **Language**: C++14 standard
2. **Naming**: Follow existing naming conventions in the codebase
3. **Headers**: Use include guards or `#pragma once`
4. **Comments**: Use Chinese for business logic comments, English for API documentation
5. **Error Handling**: Use HcclResult return codes

Refer to [CANN Community Coding Standards](https://gitcode.com/cann/community/tree/master/contributor/coding-standards) for detailed guidelines.

## CMake Configuration

### Key CMake Variables

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
| `ENABLE_ASAN` | Enable AddressSanitizer |
| `ENABLE_GCOV` | Enable code coverage |

### Build Targets

| Target | Description |
|--------|-------------|
| `hccd` | Main HCCD shared library |
| `ccl_kernel_plf` | CCL kernel platform shared library |
| `ccl_kernel_plf_a` | CCL kernel platform static library |
| `package` | Build installable package |

## Deployment

### Installation

```bash
bash ./build_out/cann-hcomm_<version>_linux-<arch>.run --full
```

### Uninstallation

```bash
bash ./build_out/cann-hcomm_<version>_linux-<arch>.run --uninstall
```

### Security Considerations

1. Do not use root for running applications (follow principle of least privilege)
2. Set umask to 0027 or higher
3. File permissions:
   - Programs: 550 (r-xr-x---)
   - Config files: 640 (rw-r-----)
   - Log files: 640 (rw-r-----)
   - Key files: 600 (rw-------)

See [SECURITY.md](SECURITY.md) for complete security guidelines.

## Version Compatibility

HCOMM follows CANN software release versions. Ensure matching versions:

- HCOMM version: 9.0.0
- Required CANN runtime: 9.0.0
- Required metadef: 9.0.0

See [release-management](https://gitcode.com/cann/release-management) for version mapping.

## Contributing

1. Sign the CLA (Contributor License Agreement)
2. Follow the coding standards
3. Submit PRs with detailed descriptions
4. For non-trivial changes, create an Issue first for discussion

See [CONTRIBUTING.md](CONTRIBUTING.md) for details.

## Troubleshooting

### Common Build Issues

1. **Missing CANN toolkit**: Set `ASCEND_HOME_PATH` or use `-p` option
2. **Missing third-party libs**: Run `build_third_party.sh` first
3. **Version mismatch**: Ensure HCOMM version matches CANN version

### Runtime Issues

Check logs and refer to error codes in `hccl_types.h` for debugging.

## References

- [HCCL Documentation](docs/README.md)
- [Build Guide](docs/build.md)
- [Examples](examples/README.md)
- [Security Declaration](SECURITY.md)
- [License](LICENSE)
