# HCCL-VM User Guide

## 1. Overview

HCCL-VM is a virtual execution environment for high-performance collective communication targeting Huawei Ascend NPU cards. This tool enables the development and functional verification of HCCL collective communication operators without real Ascend hardware.

![hccl-vm GIF](docs/hccl-vm.gif)

## 2. Prerequisites

| Dependency | Version Requirement |
|------------|---------------------|
| System Architecture | x86_64 Ubuntu 22.04 or later |
| Specification | Ascend950, for others refer to [Tool Specification Constraints](#45-tool-specification-constraints) |

### 2.1 CANN Package Installation

Install the latest CANN Toolkit development package and CANN ops operator package [Download Link](https://ascend.devcloud.huaweicloud.com/artifactory/cann-run-mirror/software/master/)

```bash
# Ensure the installation packages have executable permissions
chmod +x Ascend-cann-toolkit_9.1.0_linux-x86_64.run
chmod +x Ascend-cann-950-ops_9.1.0_linux-x86_64.run
# Installation commands
./Ascend-cann-toolkit_9.1.0_linux-x86_64.run --install --install-path=/home/workspace/Ascend
./Ascend-cann-950-ops_9.1.0_linux-x86_64.run --install --install-path=/home/workspace/Ascend
```

### 2.2 hccl_test Compilation

hccl_test is the official HCCL performance testing tool provided by Ascend. See [HCCL Performance Testing Tool](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta1/devaids/hccltool/HCCLpertest_16_0001.html). HCCL-VM supports running hccl_test cases in a virtual environment. Please first refer to the [hccl_test Case Build](#42-hccl-test-case-build) section to compile the test case binary.

Note: Optional. PyTorch test cases will be supported in the future.

---

## One-Click Installation

Complete dependency installation, source code retrieval, CANN detection, and compilation in one line (default `campus-2026` profile):

```bash
curl -fsSL https://raw.gitcode.com/cann/hcomm/raw/competition%2Fcampus-2026/test/hccl_vm/hccl_vm_installer | bash
```

You can also download and run locally (for review or offline distribution): `bash hccl_vm_installer`. Append parameters with `... | bash -s -- --workspace /root/hvm`.

**Prerequisites**: x86_64 Linux; the toolchain must meet hcomm build.md requirements — gcc/g++ 7.3.0–13.3.x, cmake ≥ 3.16.0 (applies to both host and aarch64 cross-compilers). Ubuntu 22.04 / 24.04 work out of the box. Later versions with default gcc (14/15) exceeding the range will trigger a warning; the script continues, but a compliant environment is recommended.

**CANN**: The script only probes for CANN in the working directory `<workspace>/Ascend` (or the path specified by `--ascend-path`). If not found, it automatically downloads and installs the matching version to that location, with consistent behavior for both root and regular users. `--offline` only checks, never downloads. In offline environments without public internet, it falls back to printing instructions for self-provided CANN.

**hccl_test**: By default, OpenMPI and the hccl_test performance testing tool are also compiled. Use `--skip-hccl-test` to disable.

**Common Parameters**:
- `--profile <name>`: Profile (default `campus-2026`, use `--list-profiles` to list all)
- `--workspace <path>`: Working directory for source code, compilation, and artifacts (default: current directory)
- `--ascend-path <path>`: Specify the CANN directory; reuse if exists, install if not
- `--reinstall-cann`: Re-download and overwrite existing CANN (use when version mismatch; kept by default)
- `--offline`: Use existing CANN only, never download
- `--skip-hccl-test`: Skip hccl_test compilation
- `-h`: Full help

After completion, the tool is located at `<workspace>/hcomm/test/hccl_vm/hccl_vm_install/bin/hccl-vm`. Delete the working directory to clean up all tool artifacts (system dependencies installed via apt require manual `apt remove`). This tool does not modify CANN.

---

## 3. Quick Start

### 3.1 Tool Build & Installation

```bash
# 1. Create working directory
mkdir -p /home/workspace
cd /home/workspace

# 2. Download dependency source code
git clone https://gitcode.com/cann/hccl.git
git clone https://gitcode.com/cann/hcomm.git

# 3. Install third-party dependencies
sudo apt-get update
sudo apt install build-essential cmake libsqlite3-dev rdma-core libibverbs-dev pkg-config gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-user-static binfmt-support

# 4. Compile the HCCL-VM tool. After downloading the hcomm code, the tool source is at: /home/workspace/hcomm/test/hccl_vm
cd /home/workspace/hcomm/test/hccl_vm
source /home/workspace/Ascend/cann/set_env.sh
export HCCL_CODE_HOME=/home/workspace/hccl
export HCOMM_CODE_HOME=/home/workspace/hcomm
bash ./build.sh --full
```

### 3.2 Usage Examples

#### 3.2.1 Environment Configuration

Refer to [hccl_rootinfo File Contents](#47-hccl_rootinfojson-file) to create and configure the `hccl_rootinfo.json` file.

#### 3.2.2 CCU Mode

1. Environment variable configuration.

```bash
# Enter the tool installation directory
cd /home/workspace/hcomm/test/hccl_vm/hccl_vm_install
source /home/workspace/Ascend/cann/set_env.sh
export LD_LIBRARY_PATH=$ASCEND_HOME_PATH/lib64:$ASCEND_HOME_PATH/devlib:$LD_LIBRARY_PATH
export RANK_TABLE_FILE=$(pwd)/data/ranktable.json
export HCCL_OP_EXPANSION_MODE="CCU_SCHED"
```

2. Execution

```bash
# Navigate to the new bin directory to execute hccl-vm
cd /home/workspace/hcomm/test/hccl_vm/hccl_vm_install/bin

# Select the Ascend cluster topology configuration file, start the tool, initialize the cluster environment, and enter the tool command line
./hccl-vm start ascend950_cluster_32_server_normal.yaml

# To enable the runner plugin (optional)
(hvm)$> hccl-vm plugin install @runner

# Select the communication domain configuration file for this operator execution (run hccl_test in a cluster with 1 super node, 1 Server, 1 NPU)
(hvm)$> hccl-vm mock-comm 112
(hvm)$> mpirun --allow-run-as-root --oversubscribe -np 2 ${ASCEND_HOME_PATH}/tools/hccl_test/bin/reduce_scatter_test -b 64 -e 64 -d int32 -o sum -w 0 -n 1 -c 1 > log.txt

# Execute checker validation
(hvm)$> hccl-vm plugin run @checker

# Exit the tool terminal
(hvm)$> exit
```

3. Verify hccl_test case execution results

[Runner Result Viewing](#491-runner-plugin-results)
[Checker Result Viewing](#492-checker-plugin-results)

#### 3.2.3 AICPU Mode

AICPU expansion mode requires executing algorithm expansion steps on the device side. Therefore, the hccl-vm tool needs to compile and simulate execution of HCCL's device-side symbols. Since device-side symbols use the ARM architecture, a cross-compiler is needed on x86 environments for compilation, and QEMU is required for runtime simulation of AICPU mode execution.

Device-side symbols are compiled using hccl and hcomm source code. To ensure correct Host-Device communication protocol, the Host-side installation package must also be compiled and replaced.

1. HCCL device-side symbol compilation, installation, copying, etc.

```bash
cd /home/workspace/hcomm/test/hccl_vm/
bash ./build_pkg.sh
```

2. Environment variable configuration.

```bash
# Enter the tool installation directory
cd /home/workspace/hcomm/test/hccl_vm/hccl_vm_install
source /home/workspace/Ascend/cann/set_env.sh
export LD_LIBRARY_PATH=$ASCEND_HOME_PATH/lib64:$ASCEND_HOME_PATH/devlib:$LD_LIBRARY_PATH
export RANK_TABLE_FILE=$(pwd)/data/ranktable.json
export HCCL_OP_EXPANSION_MODE="AI_CPU"
```

3. Execution

```bash
# Navigate to the new bin directory to execute hccl-vm
cd /home/workspace/hcomm/test/hccl_vm/hccl_vm_install/bin

# Select the Ascend cluster topology configuration file, start the tool, initialize the cluster environment, and enter the tool command line
./hccl-vm start ascend950_cluster_32_server_normal.yaml

# To enable the runner plugin (optional)
(hvm)$> hccl-vm plugin install @runner

# Select the communication domain configuration file for this operator execution (run hccl_test in a cluster with 1 super node, 1 Server, 1 NPU)
(hvm)$> hccl-vm mock-comm 112
(hvm)$> mpirun --allow-run-as-root --oversubscribe -np 2 ${ASCEND_HOME_PATH}/tools/hccl_test/bin/reduce_scatter_test -b 64 -e 64 -d int32 -o sum -w 0 -n 1 -c 1 > log.txt

# Execute checker validation
(hvm)$> hccl-vm plugin run @checker

# Exit the tool terminal
(hvm)$> exit
```

4. Verify hccl_test case execution results [Runner Result Viewing](#491-runner-plugin-results) [Checker Result Viewing](#492-checker-plugin-results)

#### 3.2.4 AIV Mode

1. Environment variable configuration.

```bash
# Enter the tool installation directory
cd /home/workspace/hcomm/test/hccl_vm/hccl_vm_install
source /home/workspace/Ascend/cann/set_env.sh
export LD_LIBRARY_PATH=$ASCEND_HOME_PATH/lib64:$ASCEND_HOME_PATH/devlib:$LD_LIBRARY_PATH
export RANK_TABLE_FILE=$(pwd)/data/ranktable.json
export HCCL_OP_EXPANSION_MODE="AIV"
```

2. Execution

```bash
# Navigate to the new bin directory to execute hccl-vm
cd /home/workspace/hcomm/test/hccl_vm/hccl_vm_install/bin

# Select the Ascend cluster topology configuration file, start the tool, initialize the cluster environment, and enter the tool command line
./hccl-vm start ascend950_cluster_32_server_normal.yaml

# To enable the runner plugin (optional)
(hvm)$> hccl-vm plugin install @runner

# Select the communication domain configuration file for this operator execution (run hccl_test in a cluster with 1 super node, 1 Server, 1 NPU)
(hvm)$> hccl-vm mock-comm 112
(hvm)$> mpirun --allow-run-as-root --oversubscribe -np 2 ${ASCEND_HOME_PATH}/tools/hccl_test/bin/reduce_scatter_test -b 64 -e 64 -d int32 -o sum -w 0 -n 1 -c 1 > log.txt

# Execute checker validation
(hvm)$> hccl-vm plugin run @checker

# Exit the tool terminal
(hvm)$> exit
```

3. Verify hccl_test case execution results [Runner Result Viewing](#491-runner-plugin-results) [Checker Result Viewing](#492-checker-plugin-results)

### 3.3 PyTorch Example

Not yet supported.

### 3.4 hccl Code Modification Verification Example

If you modify CANN operator package code, such as adding a new algorithm type, follow these steps to apply your changes. The `build_pkg.sh` script helps you build, install, and copy device-side dependency symbols. Set the environment variables before execution:

```bash
# Assume your CANN installation directory is: /home/workspace/Ascend
source /home/workspace/Ascend/cann/set_env.sh
# Configure hccl code repository path
export HCCL_CODE_HOME=/home/workspace/hccl
# Configure hcomm code repository path
export HCOMM_CODE_HOME=/home/workspace/hcomm
```

1. If you modified the CANN hccl repository code, run `bash build_pkg.sh --install hccl`.
2. If you modified the CANN hcomm repository code, run `bash build_pkg.sh --install hcomm`.
3. Refer to the [Usage Examples](#32-usage-examples) section and re-run the test cases.

---

## 4. Detailed Guide

### 4.1 Tool Environment Variable Configuration

**HCCL-VM Environment Variables**:

| Environment Variable | Purpose | Example |
|----------------------|---------|---------|
| `HCCL_CODE_HOME` | Specifies the HCCL source code path for HCCL-VM compilation. Not configured by default. | `export HCCL_CODE_HOME=/home/workspace/hccl` |
| `HCOMM_CODE_HOME` | Specifies the HCOMM source code path for HCCL-VM compilation. Not configured by default. | `export HCOMM_CODE_HOME=/home/workspace/hcomm` |
| `HCCLVM_ENABLE_DUMP_DATA` | Enables Runner plugin to dump input & output data. When enabled, each operator's input & output data is dumped to the `all_rank_input_output.txt` file during test execution. | `export HCCLVM_ENABLE_DUMP_DATA=1` to enable, `export HCCLVM_ENABLE_DUMP_DATA=0` to disable |

### 4.2 HCCL-Test Case Build

The hccl_test source code is located in the CANN package installation directory. It supports compilation and execution in both OpenMPI and MPICH environments. See [OpenMPI and MPICH Environment Case Execution Differences](#48-differences-in-running-cases-between-openmpi-and-mpich-environments) for runtime differences. This guide uses OpenMPI as an example.

#### 4.2.1 OpenMPI Environment Compilation

1. Install OpenMPI

```bash
sudo apt-get update
sudo apt install openmpi-bin libopenmpi-dev
```

2. Compile hccl_test

```bash
# Change CANN installation directory permissions
chmod -R 755 /home/workspace/Ascend

# Enter the hccl_test source code directory
cd /home/workspace/Ascend/cann/tools/hccl_test

# Set CANN environment variables
source /home/workspace/Ascend/cann/set_env.sh

# Temporarily modify the Makefile script
if ! grep -q '\-lmpi_cxx' Makefile; then
    sed -i 's/-lmpi/-lmpi -lmpi_cxx/g' Makefile
fi

# Compile hccl_test cases
MPI_HOME=/usr/lib/x86_64-linux-gnu/openmpi make ASCEND_DIR=${ASCEND_HOME_PATH}
```

#### 4.2.2 MPICH Environment Compilation

Assume the mpich path is: `/usr/lib/mpich`.

```bash
# Enter the hccl_test source code directory
cd /home/workspace/Ascend/cann/tools/hccl_test

# Set CANN environment variables
source /home/workspace/Ascend/cann/set_env.sh

# Configure environment variables
export LD_LIBRARY_PATH=/usr/lib/mpich/lib/:${ASCEND_HOME_PATH}/lib64/:${ASCEND_HOME_PATH}/x86_64-linux/devlib:$LD_LIBRARY_PATH

# Compile hccl_test cases
make MPI_HOME=/usr/lib/mpich/ ASCEND_DIR=${ASCEND_HOME_PATH}
```

### 4.3 Ascend Cluster Topology Configuration File Description

#### 4.3.1 Server/Pod Topology Configuration File Description

An Ascend cluster topology is composed of one or more Server/Pod sub-topologies combined according to CLOS hierarchical network rules. Therefore, before generating a cluster topology, users need to confirm the topology type of each Server/Pod.
Users can either use predefined topology types provided by the HCCL-VM tool or define custom Server/Pod topology types according to the configuration file format requirements.

Describing the topological network relationship of a Server/Pod mainly includes the following aspects:

- **Port Allocation Table**: Describes the physical port configuration of an NPU card, such as NPU-to-NPU direct ports (P2P), NPU out-of-chassis ports (P2NET), etc.
- **Link Configuration Table**: Describes the connection relationships among all NPU cards within a Server/Pod, such as full mesh.
- **PortBound**: Describes the binding relationship of certain ports on an NPU card, where multiple ports are bound into a PortGroup.

```yaml
type: "server_intra_links"
name: "ascend950_links_topo_demo"
description: "ascend950 chip standard topology connection relationship description file"

soc_version: "Ascend950"
device_num: 16

device_ports_allocate_map:
 # port allocation table: 0: unused, 1: device direct connect, 2: device to switch, 3: d2h port
 #                 portId:  0  1  2  3  4  5  6  7  8
    - {die_id: 0, pin_map: [1, 1, 1, 0, 2, 2, 2, 2, 3]} # die0
    - {die_id: 1, pin_map: [0, 0, 0, 0, 0, 0, 0, 0, 0]} # die1

# port_group: describes which ports are merged into a portGroup. Ports in the same portGroup share the same IP address.
port_group:
    - {layer: 0, ports: ["0/4", "0/5", "0/6", "0/7"]}

links:
  # ── Method: every 8 devices form full interconnect ──
  - link_mode: "fullmesh"
    connections:
      # The following example shows that die0 devices 0, 1, 2, 3 are all fully connected via die0 ports
      - {die_id: 0, devices_range: [0, 3]}
      - {die_id: 0, devices_range: [4, 7]}
      - {die_id: 0, devices_range: [8, 11]}
      - {die_id: 0, devices_range: [12, 15]}
  
  #- link_mode: "enum"
  #  device_to_device_links:
  #    The following example shows that device 0 and 1 both connect via die0 ports to devices 1, 3, 5, 7 on die1 ports respectively.
  #    i.e.: device0 connects to device1, device3, device5, device7; device1 connects to device3, device5, device7
  #    - {src_die_id: 1, src_local_id_range: [0, 2], dst_die_id: 1, dst_local_id_range: [1, 3, 5, 7]}

  - link_mode: "enum"
    device_to_switch_links:
      # The following example shows that devices 0 to 15 all connect to the switch via die0 ports. Combined with portGroup, device0 to device15 all connect to the switch via portGroup[0/4, 0/5, 0/6, 0/7].
      - {die_id: 0, devices_range: [0, 15]}
```

**Field Descriptions**:

- **soc_version**: Chip model, e.g., `Ascend950`.
- **device_num**: Total number of devices, determined by chip model and topology type.
- **device_ports_allocate_map**: Port allocation table describing each die's port configuration. 1 represents device direct connect ports, 2 represents device-to-switch ports, 3 represents d2h ports.
- **port_group**: Describes which ports are merged into a portGroup. Ports in the same portGroup share the same IP address. Ports not configured default to one port per portGroup.
- **links**: Link configuration table describing the connection relationships among all NPU cards within a Server/Pod, and between NPUs and switches.
  - **NPU direct connections**: The tool provides two methods for configuring NPU direct connections:
   - **link_mode == "fullmesh"**: Indicates all devices are fully connected based on one die's ports. New typical connection modes can be added as new link_mode types, such as "ring".
   - **link_mode == "enum"**: Enumeration method. When the NPU connection method within a Server/Pod is complex, all link relationships can be described through enumeration.
  - **NPU-to-switch connections**: Users can configure NPU-to-switch connection relationships using the enumeration method.
- **device_to_device_links**: Describes NPU-to-NPU connection relationships.
- **device_to_switch_links**: Describes NPU-to-switch connection relationships.

#### 4.3.2 Cluster Topology Configuration File Description

An Ascend cluster network is composed of one or more Server/Pod sub-topologies combined according to CLOS hierarchical network rules. Users can choose different Server/Pod topology types based on cluster size and requirements.

Users can define custom cluster topology configuration files according to the following format:

```yaml
name: "ascend950_cluster_32_server_normal"
description: "Ascend950 normal networking: 32 super nodes, 1 server per super node"

# Total number of super nodes
super_node_num: 4
# Total number of servers/pods
server_num: 32
server_list:
  # 0-7 servers: all use ascend950_server_topo_normal topology type
  - {super_pod_id: 0, id_range: [0, 7], soc_version: "Ascend950", server_topo: "ascend950_server_topo_normal.yaml"}
  - {super_pod_id: 1, id_range: [0, 7], soc_version: "Ascend950", server_topo: "ascend950_server_topo_normal.yaml"}
  - {super_pod_id: 2, id_range: [0, 7], soc_version: "Ascend950", server_topo: "ascend950_server_topo_normal.yaml"}
  - {super_pod_id: 3, id_range: [0, 7], soc_version: "Ascend950", server_topo: "ascend950_server_topo_normal.yaml"}
```

The configuration file above describes a cluster topology with 4 super nodes, 32 Servers, and a total of 128 NPU cards. Each Server/Pod uses the `ascend950_server_topo_normal` topology type.

**Field Descriptions**:

- **super_node_num**: Total number of super nodes.
- **server_num**: Total number of servers/pods.
- **server_list**: Configuration information for each Server/Pod, including super node ID, device ID range, chip model, and Server/Pod topology configuration file path.

#### 4.3.3 Communication Domain Configuration File Description

In an Ascend cluster environment, users need to select different communication domain configuration files based on the communication domain required by the operator to be executed.

The tool provides the `hccl-vm mock-comm` command to read and configure operator communication domain configuration files. The communication domain configuration file format is yaml, located at `hccl_vm_install/config/topo_meta`. If the corresponding configuration file does not exist in the directory, the user needs to create one first.

The hccl-vm tool supports asymmetric topology communication domain configuration, as shown below:

```yaml
# 1. Global statistics: podNum, serNum, rankNum are all less than 1024
meta:
  podNum: 1  # Total number of super nodes
  serNum: 2  # Total number of servers
  rankNum: 6 # Total number of ranks

# 2. Detailed topology structure
topology:
  - podId: 0
    servers:
      - serId: 0
        # Local IDs of ranks running on this server
        ranks: [0, 2]
      - serId: 1
        # Local IDs of ranks running on this server
        ranks: [1, 3, 5, 7]
```

**Notes**:

- When configuring the communication domain, the tool regenerates the `topo.json` and `ranktable.json` files based on the specified communication domain configuration ID.
- In the communication domain configuration yaml file above, the `ranks` field represents the list of local IDs (i.e., device physical IDs) of ranks actually running on each server.

#### 4.3.4 topo and ranktable.json File Description

The `topo.json` and `ranktable.json` files do not need to be created manually. The tool automatically generates them based on the following information:

- **Topology configuration ID**: The ID specified by the user at startup (e.g., 112, 113, etc.)
- **Chip type**: The chip type automatically identified based on the runtime environment.

Although the configuration files are automatically generated by the tool, understanding their structure helps in understanding topology configuration.

**topo.json Structure**:

`topo.json` describes the connection relationships among all devices within a server:

```json
{
  "server": {
    "device_count": 8,
    "groups": [
      {
        "group_id": 0,
        "device_start": 0,
        "device_count": 8,
        "topo_layout": "1D"
      }
    ]
  },
  "ports": [
    {
      "ccu": "die0",
      "port_pattern": "0/{0-6}",
      "protocol": "HCCS",
      "func_id": 2,
      "usage": "peer2peer",
      "ip_binding": "independent"
    },
    {
      "ccu": "die0",
      "port_pattern": "0/7,0/8",
      "protocol": "ROCE",
      "func_id": 3,
      "usage": "peer2net",
      "ip_binding": "independent"
    }
  ],
  "links": [
    {
      "net_layer": 0,
      "link_type": "PEER2PEER",
      "topo_type": "1DMESH",
      "ccu": "die0",
      "port_pattern": "0/{0-6}",
      "connect_pattern": "full_mesh",
      "group_id": 0
    },
    {
      "net_layer": 1,
      "link_type": "PEER2NET",
      "topo_type": "CLOS",
      "ccu": "die0",
      "port_pattern": "0/7,0/8",
      "connect_pattern": "all_to_net",
      "group_id": 0
    }
  ]
}
```

**Field Descriptions**:

- `server.device_count`: Total number of devices.
- `server.groups`: Device grouping information.
- `ports`: Port configuration.
  - `usage`: Port purpose (`peer2peer` for inter-device connections, `peer2net` for external connections)
- `links`: Link configuration.
  - `link_type`: Link type (`PEER2PEER` or `PEER2NET`)
  - `topo_type`: Topology type (`1DMESH`, `CLOS`, etc.)

**ranktable.json Structure**:

`ranktable.json` describes the device and IP mapping for the current run:

```json
{
  "version": "1.0",
  "server_count": 1,
  "device_count": 8,
  "server_list": [
    {
      "server_id": 0,
      "device_id": 0,
      "device_ip": "192.168.1.10",
      "port": "2222"
    }
  ]
}
```

**Field Descriptions**:

- `server_count`: Number of servers.
- `device_count`: Total number of devices.
- `server_list`: Server and device list.
  - `device_ip`: Device IP address.
  - `port`: Device port number.

### 4.4 hccl_config.sh File Description

The `hccl_config.sh` file contains the environment variable configuration required for running HCCL_Test cases. The environment variables are consistent with those used for Hccl_Test cases on real hardware.
Users need to modify the `hccl_config.sh` script according to their own use cases and requirements to configure the HCCL test case runtime environment variables.

```bash
#!/bin/bash
# hccl_config.sh - HCCL environment variable configuration

remove_files_by_prefix() {
  if [ "$#" -ne 1 ]; then
    echo "Usage: remove_files_by_prefix <prefix>" >&2
    return 2
  fi

  local prefix="$1"
  if [ -z "$prefix" ]; then
    return 0
  fi

  shopt -s nullglob
  local any_deleted=0
  for f in "${prefix}"*; do
    if [ -f "$f" ]; then
      rm -f -- "$f" && any_deleted=1
    fi
  done
  shopt -u nullglob

  # Return 0 regardless of whether files were deleted, to ensure script continues
  return 0
}

# Clean up redundant files in the data/ directory (temporary files generated in CCU mode)
cd "${HCCL_VM_INSTALL_DIR}/data" 2>/dev/null && {
  remove_files_by_prefix "sqe_info_rank_"
  remove_files_by_prefix "mc_instr_info_rank_"
  rm -f "all_rank_input_output.txt"
  cd "${HCCL_VM_INSTALL_DIR}"
}

# Set CANN environment variables
source /home/workspace/Ascend/cann/set_env.sh

# Disable HCCL heartbeat function
export HCCL_DFS_CONFIG=cluster_heartbeat:off

# Set HCCL-VM installation path, inferred from the script itself (compatible with bin/ and script/ subdirectories)
_INSTALL_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
case "$(basename "${_INSTALL_SCRIPT_DIR}")" in
    bin|script)
        export HCCL_VM_INSTALL_DIR="$(dirname "${_INSTALL_SCRIPT_DIR}")"
        ;;
    *)
        export HCCL_VM_INSTALL_DIR="${_INSTALL_SCRIPT_DIR}"
        ;;
esac
unset _INSTALL_SCRIPT_DIR

# Configure LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$ASCEND_HOME_PATH/lib64:$ASCEND_HOME_PATH/devlib:$LD_LIBRARY_PATH

# Set ranktable.json file path (consistent with mock-comm generation path)
export RANK_TABLE_FILE=${HCCL_VM_INSTALL_DIR}/data/ranktable.json

# Set log level
export ASCEND_GLOBAL_LOG_LEVEL=1

# Enable log output to stdout
export ASCEND_SLOG_PRINT_TO_STDOUT=1

# Set HCCL operation mode (CCU, AI_CPU, AIV, etc.)
export HCCL_OP_EXPANSION_MODE="CCU_SCHED"
# Or set HCCL runtime parameters (AI_CPU expansion mode) AI_CPU mode environment variables cannot be set simultaneously with other modes
# export HCCL_OP_EXPANSION_MODE="AI_CPU"
# Or set HCCL runtime parameters (AIV expansion mode) AIV mode environment variables cannot be set simultaneously with other modes
# export HCCL_OP_EXPANSION_MODE="AIV"

echo "HCCL-VM environment configured successfully!"
```

### 4.5 Tool Specification Constraints

**Supported Operator Types**:

The supported operator types include: allgather/allreduce/alltoall/reduce/reduce\_scatter/scatter/alltoallv.

**Supported Data Types**:

The HCCL-VM tool supports the following data types: int8/int16/int32/fp16/fp32/uint8/uint16/uint32/bfp16/hif8/fp8e4m3/fp8e5m2/fp8e8m0.

The HCCL-VM Runner plugin supports the following data types:

| ReduceOp | DataType                           |
|----------|------------------------------------|
| `ADD`    | `int8/int16/int32/uint8`           |
| `MIN`    | `int8/int16/int32/uint8`           |
| `MAX`    | `int8/int16/int32/uint8`           |

**Hardware Specifications**:

Currently, this tool only supports the Ascend950 chip. A single server supports a maximum of 8 cards. For more than 8 cards, cross-server execution is required.

### 4.6 HCCL-VM Plugin Features

#### 4.6.1 Runner Plugin

The Runner plugin simulates the execution of task sequences generated by HCCL operator orchestration and outputs data.
The simulation runner plugin is **disabled** by default during hccl_test case execution. After the hccl_test case calls the operator interface, it waits for the operator task to complete via the `aclrtSynchronizeStream` interface. The simulation runner tool waits until all ranks are in a waiting state, then starts simulating the execution of all rank tasks. After execution completes, it notifies each rank's test case to continue.

After test case execution, users can view the input buffer and output buffer data for each rank in the `all_rank_input_output.txt` file in the execution directory. This feature is disabled by default and can be enabled via the corresponding command before test execution.

**Installation and Uninstallation**:

The Runner plugin supports installation and uninstallation via the `hccl-vm plugin install/uninstall` command. The runner plugin must be installed after entering the hccl-vm tool command line and before executing the test case. Subsequent executions will then run the runner.

```bash
# Install runner plugin
(hvm)$> hccl-vm plugin install @runner

# Uninstall runner plugin
(hvm)$> hccl-vm plugin uninstall @runner
```

#### 4.6.2 Checker Plugin

The Checker plugin is an algorithm analyzer plugin. It forms a DAG from all tasks generated by HCCL and analyzes it to detect memory conflicts. By simulating execution of the DAG, it also detects semantic errors.

The Checker plugin is started manually by the user via a command.

The Checker plugin is currently in a transition period. Checker V3 is a refactored version of the original Checker, mainly improving validation performance. By default, the new Checker (Checker V3) is used. You can adjust this by modifying the configuration parameters in the Checker's `manifest.json` file.

```bash
# Configuration file located at /pathto/hccl_vm_install/plugin/checker/manifest.json

{
  "name": "checker",        // Checker plugin name
  "version": "1.0.0",       // Checker plugin version
  "entry": "./checker",     // Checker plugin startup command
  "dependency": {
      "min_core_version": "1.0.0"
  },
  "setting": {              // Checker plugin configuration
      "enable_new_checker": true,           // Whether to enable the new Checker (Checker V3, enabled by default)
      "enable_old_checker": false,          // Whether to enable the old Checker (disabled by default)
      "enable_insight_dump": false,         // Whether to enable visualization data output (disabled by default, only supported by old Checker)
      "enable_memory_snapshot_dump": false  // Whether to enable visualization memory snapshot data output (disabled by default, only supported by old Checker, requires visualization data output "enable_insight_dump" to be enabled first)
  }
}
```

### 4.7 hccl_rootinfo.json File

Currently, the tool uses the `ranktable.json` file for communication domain initialization. Therefore, the `hccl_rootinfo.json` file is only needed to provide the path to the `topo.json` file.
If the `hccl_rootinfo.json` file does not exist under the `/etc` path, users need to create it with the following content:

```json
{
  "version": "2.0",
  "topo_file_path": "/home/workspace/hcomm/test/hccl_vm/hccl_vm_install/data/topo.json"
}
```

### 4.8 Differences in Running Cases Between OpenMPI and MPICH Environments

Before running hccl_test cases, users can use the `which` command to determine which mpirun is being used in the current environment.

#### 4.8.1 Environment Variable Configuration Differences

OpenMPI is generally the default configuration in most environments. If using OpenMPI to run cases, no additional environment variable configuration is typically needed.
If using the MPICH environment to run cases, environment variables must be configured as follows:

```bash
# Configure mpich environment variables
export LD_LIBRARY_PATH=/usr/lib/mpich/lib/:${ASCEND_HOME_PATH}/lib64/:${ASCEND_HOME_PATH}/x86_64-linux/devlib:$LD_LIBRARY_PATH
export PATH=/usr/lib/mpich/bin:$PATH
```

#### 4.8.2 mpirun Command Parameter Differences

In the OpenMPI environment, run hccl_test cases as follows:

```bash
export HCCL_TEST_PATH=/home/workspace/Ascend/cann/tools/hccl_test
mpirun --allow-run-as-root --oversubscribe -np 2 ${HCCL_TEST_PATH}/bin/reduce_scatter_test -b 64 -e 64 -d int32 -o sum -w 0 -n 1 -c 1
```

**Parameter Descriptions**:

- --allow-run-as-root: OpenMPI-specific parameter that allows MPI processes to run as the root user, for use in environments without root privileges.
- --oversubscribe: OpenMPI-specific parameter, removes CPU slot limits, allowing a single node to launch processes when the number of processes exceeds the number of logical CPU cores—i.e., running with oversubscription/excess allocation.
- -np 2: Specifies 2 processes, consistent with the number of nodes.

In the MPICH environment, run hccl_test cases as follows:

```bash
export HCCL_TEST_PATH=/home/workspace/Ascend/cann/tools/hccl_test
mpirun -np 2 ${HCCL_TEST_PATH}/bin/reduce_scatter_test -b 64 -e 64 -d int32 -o sum -w 0 -n 1 -c 1
```

**Parameter Descriptions**:

- -np 2: Specifies 2 processes, consistent with the number of nodes.

### 4.9 Result Viewing

#### 4.9.1 Runner Plugin Results

If the runner plugin is installed via `hccl-vm plugin install @runner` in the hccl-vm terminal, the runner plugin is automatically triggered after operator execution completes. The final result depends on hccl_test verification. Users should check the redirected log file for `[error]` level logs and the final verification result:

```bash
data_size(Bytes): | aveg_time(us): | alg_bandwidth(GB/s): | check_result:
64                | 1000.00        | 0.00006              | success
```

#### 4.9.2 Checker Plugin Results

After executing `hccl-vm plugin run @checker` in the hccl-vm terminal, the Checker validation process and results are printed to the terminal. Users should check for `[error]` level logs and the final validation result:

```bash
[info][PID:144373][TID:144880][main.cc][RunChecker] [RunChecker] op[0] Checker Success.
```

---

### 4.10 Large Memory Reuse (Check-Only Mode)

Check-only mode is used for scenarios where only Checker validation is needed in large-scale clusters. When enabled, a single large memory allocation of 200MB to 4GB reuses the same 4GB shared pool `HcclCommPool`, shared across all ranks and allowing mutual overwriting. This significantly reduces `/dev/shm` usage. In this mode, the content of large blocks is not guaranteed to be correct, making it suitable only for the Checker V3 validation pipeline that does not read buffer data. Do not enable this mode when numerically correct results are needed.

Check-only mode is a session-level switch. Append `--check-only` after the `start` subcommand to explicitly enable it. Without this flag, the default normal mode is used, where large blocks use real independent allocation with no correctness impact. Allocations smaller than 200MB always use real allocation. A single block exceeding 4GB in check-only mode results in a direct error. Check-only mode does not conflict with Runner, but if Runner is installed while check-only mode is enabled, large block reuse still takes effect and may overwrite Runner data. The tool prints a warning in this case.

```bash
# Start the tool with check-only mode enabled
./hccl-vm start ascend950_cluster_32_server_normal.yaml --check-only
```

---

## 5 Appendix

### Open Source Third-Party Software Dependencies

When compiling this project, the following third-party open-source software is required. For offline compilation, download and rename the packages, then place them in the `third_party` directory under the project.

| Open Source Software | Version | Download URL |
|---------------------|---------|--------------|
| CLI11               | 2.2.0   | [cli11-2.2.0.tar.gz](https://raw.gitcode.com/src-openeuler/cli11/blobs/58c912141164a5c0f0139bfa91343fefe151d525/cli11-2.2.0.tar.gz) |
| json                | 3.11.3  | [include.zip](https://gitcode.com/cann-src-third-party/json/releases/download/v3.11.3/include.zip) |
| spdlog              | 1.11.0  | [spdlog-v1.11.0.tar.gz](https://raw.gitcode.com/src-openeuler/spdlog/blobs/c2dfb1aca26c607393665c836155613ff283de66/v1.11.0.tar.gz) |
| yaml-cpp            | 0.8.0   | [yaml-cpp-0.8.0.tar.gz](https://raw.gitcode.com/src-openeuler/yaml-cpp/blobs/d1ead4fff417073b9cdbf98b8b55eb0efc00b0ba/yaml-cpp-0.8.0.tar.gz) |
| sqlite              | 3.51.0  | [sqlite-amalgamation-3510300.zip](https://www.sqlite.org/2026/sqlite-amalgamation-3510300.zip) |
| googletest          | 1.14.0  | [googletest-1.14.0.tar.gz](https://gitcode.com/cann-src-third-party/googletest/releases/download/v1.14.0/googletest-1.14.0.tar.gz) |

### Glossary

| Term       | Description                                                      |
|------------|------------------------------------------------------------------|
| HCCL       | Huawei Collective Communication Library                          |
| NPU        | Neural Processing Unit                                           |
| CANN       | Compute Architecture for Neural Networks, Huawei Ascend AI processor software stack |
| MPI        | Message Passing Interface                                        |
| CCU        | Collective Communication Unit                                    |
| Topology   | Device connection relationship                                   |
| Rank       | Process identifier in MPI                                        |

---

**Document Version**: v1.1.
**Last Updated**: 2026-06-30.
