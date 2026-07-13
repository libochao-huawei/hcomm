# HVRM Insight V3 User Guide

## 1. Overview

HVRM Insight V3 currently mainly supports `DAGView` task graph viewing. This document focuses on common operations around DAGView, including data preparation, page access, dataset selection, DAG graph browsing, and node search.

Other pages and advanced linked capabilities are not yet implemented.

---

## 2. Prerequisites

Before using Insight V3, ensure Checker has been executed and Insight data has been generated.

To configure Checker data output, ensure Insight dump is enabled in the Checker configuration file:

```json
# Configuration file located at /pathto/hccl_vm_install/plugin/checker/manifest.json

{
  ...
  "setting": {              // Checker plugin configuration
      ...
      "enable_insight_dump": true,         // Whether to enable visualization data output (disabled by default)
      "enable_memory_snapshot_dump": false  // Whether to enable visualization memory snapshot data output (disabled by default, only supported by old Checker, requires "enable_insight_dump" to be enabled first)
  }
}
```

DAGView requires at least the following data files:

```text
<dataset_name>/
├── manifest.json
└── graph/
    ├── graph.msgpack
    └── layout.msgpack
```

`manifest.json` is used for reading dataset metadata, while `graph.msgpack` and `layout.msgpack` are used for rendering the DAG task graph.

---

## 3. Compilation and Installation of Insight Plugin

The Insight V3 frontend source code is located at:

```text
{hccl_vm directory}/src/plugin/insight/frontend_v3
```

Before installing the Insight plugin, first complete the frontend compilation, then execute the HCCL VM build and installation process.

Note: Compiling the Insight visualization frontend requires `Node.js` and `npm`. It is recommended to use `Node.js 20.19.0` or later and `npm 10.x` or later.

Recommended steps:

1. Enter the frontend directory:

```bash
cd {hccl_vm directory}/src/plugin/insight/frontend_v3
```

2. Install frontend dependencies:

```bash
npm install
```

3. Compile the frontend artifacts:

```bash
npm run build
```

After frontend compilation completes, the `src/plugin/insight/dist/` directory is generated.

4. Return to the HCCL_VM root directory and execute `build.sh` to compile and install hccl-vm:

```bash
cd {HCCL_VM directory}
bash build.sh --package-path <ASCEND_CANN_PATH> --hcomm-path <HCOMM_CODE_PATH>
```

To package the installation directory, append `--pkg`. If the current build scenario requires AICPU / AIV / FULL mode, append `--aicpu`, `--aiv`, or `--full` as per the project's standard process.

---

## 4. Insight Plugin Configuration, Start and Stop

The Insight plugin configuration file is located at:

```text
hccl_vm_install/plugin/visualization/insight/manifest.json
```

The current default configuration is:

```json
{
    "name": "insight",
    "version": "1.0.0",
    "entry": "python3 server.py",
    "dependency": {
        "min_core_version": "1.0.0"
    },
    "setting": {
        "dist_path": "./dist",
        "data_path": "../../checker/data/insight",
        "topo_config_path": "../../../../../asset/cluster_model/config/cluster",
        "port": 8080
    }
}
```

Common field descriptions:

| Field | Description |
|-------|-------------|
| `entry` | Insight plugin startup command; currently starts the service via `python3 server.py` |
| `setting.dist_path` | Frontend static resource directory; reads `./dist` by default |
| `setting.data_path` | Insight data directory; defaults to Checker output `data/insight` |
| `setting.topo_config_path` | Cluster topology configuration directory |
| `setting.port` | Insight service port; defaults to `8080` |

To change the port or data directory, directly edit the `setting` fields in this `manifest.json`.

> Warning: Insight binds to `127.0.0.1` by default via `python3 server.py`. To listen on a specific IP instead of the default `localhost (127.0.0.1)`, enter the `hccl_vm_install/plugin/visualization/insight` directory and manually execute:
>
> ```bash
> python3 server.py --host <ip> --port <port>
> ```
>
> For example:
>
> ```bash
> cd hccl_vm_install/plugin/visualization/insight
> python3 server.py --host 0.0.0.0 --port 8080
> ```

Plugin installation and uninstallation commands:

```bash
# Install and start the Insight plugin
hccl-vm plugin install @insight

# Uninstall the Insight plugin
hccl-vm plugin uninstall @insight
```

---

## 5. Opening Insight

If the Insight service is already started, open the service address directly in your browser, for example:

```text
http://localhost:8080
```

To start Insight via the plugin, execute:

```bash
hccl-vm plugin install @insight
```

After the plugin starts, the terminal outputs the access address. Open the address in your browser to enter the Insight page.

![Launch Insight and Open Page](insight_image/V3_image/launch.gif)

---

## 6. Selecting a Dataset

After opening Insight, you enter the `Overview` page by default. Follow these steps to select a dataset for analysis:

1. View the dataset list in the center dataset table.
2. Click the target dataset.
3. Confirm the ranks to view in the Rank tree on the left.
4. If the DAG graph is large, reduce the rank selection range first.
5. Click `Enter Correlation View` on the right.

![Select Dataset and Enter Correlation View](insight_image/V3_image/dagView_V3.gif)

It is recommended to start with a small number of ranks for initial analysis, then expand the rank range after confirming the analysis direction.

---

## 7. Entering DAGView

After entering the `Correlation` page, focus on the lower half's `DAGView Task View`.

The main areas of the page are:

| Area | Function |
|------|----------|
| Left panel | Select ranks, search nodes |
| Center bottom | Display the DAG task graph |
| Right details panel | View current node details |

If the upper memory view is empty, it does not affect the viewing and use of the DAGView below.

---

## 8. Viewing the DAG Task Graph

The DAG task graph consists of swimlanes, nodes, and arrows:

| Element | Description |
|---------|-------------|
| Swimlane | Represents an execution queue, typically displayed as `Rank / Stream / Queue` |
| Node | Represents a task, such as data copy, Reduce, Record, Wait, etc. |
| Arrow | Represents dependency relationships between tasks |
| Loop dashed box | Represents a Loop region, enclosed by a dashed box with a `Loop` label outside |

Common node types:

| Node Type | Meaning |
|-----------|---------|
| `TRANS_MEM` | Data copy task |
| `REDUCE` | Reduce task |
| `RECORD` | Notify record task |
| `WAIT` | Notify wait task |
| `CCU_GRAPH` | CCU graph task |
| `AIV_GRAPH` | AIV graph task |

Recommended viewing approach:

1. Follow the arrow direction to view task dependency order.
2. Click on a task node of interest.
3. View the node's Rank, Stream, Queue, and detailed information in the right details panel.
4. Continue viewing the node's parent and child nodes to trace upstream and downstream dependencies.

For Loop display, pay additional attention to the following:

1. If a Loop exists in the DAG, the page automatically overlays a Loop dashed box on the task graph.
2. The dashed box covers the Loop's start, end, and internal nodes, making it easy to identify loop boundaries.
3. Clicking the Loop capsule directly locates the corresponding Loop Start node.
4. After selecting a Loop internal node, the right details panel shows the Loop information and nested chain for that node.

![Browse Task Graph in DAGView and View Node Details](insight_image/V3_image/dagView_V3_2.gif)

---

## 9. Canvas Operations

The DAGView canvas supports the following operations:

| Operation | Description |
|-----------|-------------|
| Move canvas | Drag the blank area |
| Zoom in / out | Use the mouse wheel, or click `+` / `-` in the bottom right |
| Reset zoom | Click the percentage button in the bottom right |
| Select node | Click the target node |

When the graph is large, first reduce the rank selection range, then zoom into the local area to view dependencies.

---

## 10. Viewing Node Details

After clicking a DAG node, the right details panel displays node information. Common information includes:

| Section | Description |
|---------|-------------|
| Node Overview | Node ID, task type, Rank, Stream, Queue |
| Loop Info | The Loop the current node belongs to, Loop count, instruction range, Loop boundaries, etc. |
| Parent Nodes | Upstream nodes the current node depends on |
| Child Nodes | Downstream nodes that depend on the current node |
| Node Semantics | Notify, Task metadata, Memory Slices, etc. |
| Raw JSON | Complete raw information for the current node |

When investigating dependency relationships, prioritize checking `Parent Nodes` and `Child Nodes`. When investigating loop structures, focus on `Loop Info`. When investigating data copy issues, focus on `Memory Slices`.

The `Memory Slices` section has the following display rules:

1. Normal memory slices display `rank / type / offset / size`.
2. If the slice belongs to `MS_CCU`, Insight V3 preferentially displays the `MSID` instead of the underlying abstract offset.
3. For batch tasks with multiple `MS_CCU` slices, they are automatically merged into a summary card.
4. The summary card shows a description like `8 MSIDs used in total`.
5. Expanding `MSID Details` shows each `MSID`'s corresponding `id` and `size`.

---

## 11. Searching for Nodes

The left `Search` panel can be used to quickly locate DAG nodes.

Supported search fields:

| Field | Use Case |
|-------|----------|
| `taskId` | Use when the node ID is known |
| `taskType` | Search by task type, e.g., `TRANS_MEM` |
| `notifyId` | Search by notify ID |

Steps:

1. Select a search field in the left search panel.
2. Enter the complete keyword.
3. Click the search result.
4. DAGView automatically locates and selects the corresponding node.

The current search is exact match. If no results are found, confirm the keyword is complete and that the target rank is selected.

---

## 12. Recommended Analysis Flow

We recommend the following DAGView analysis flow:

1. Select a dataset on the `Overview` page.
2. Select a small number of ranks in the left Rank tree.
3. Click `Enter Correlation View`.
4. View the overall nodes and dependency arrows in DAGView.
5. Click nodes of interest and view details on the right.
6. Trace upstream and downstream dependencies through parent and child nodes.
7. Use the search function to quickly locate known nodes.

---

## 13. Frequently Asked Questions

### 13.1 No Data on the Page

Confirm the Insight service address is correct and that Checker has generated Insight data.

### 13.2 DAG is Empty After Entering Correlation Page

Confirm the following files exist in the dataset:

```text
graph/graph.msgpack
graph/layout.msgpack
```

### 13.3 DAG Graph is Too Dense

First reduce the number of selected ranks on the left, view only some ranks, then zoom into local areas for analysis.

### 13.4 Cannot Find a Node via Search

Confirm the search keyword is complete and that the target node's rank is selected.
