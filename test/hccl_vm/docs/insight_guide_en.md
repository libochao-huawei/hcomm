# HVRM Insight User Guide

## 1. Overview

HVRM Insight is a distributed operator visualization analysis tool for unified display and linked analysis of graph structures, memory snapshots, and verification error information during the execution of HCCL collective communication operators. The tool includes three pages:

| Page | Tab | Purpose |
|------|-----|---------|
| **Dashboard** | Overview | Browse datasets, select ranks, view statistics; the starting point for using the tool |
| **MemView** | Correlation | Linked display of memory timeline and DAG task graph; the core analysis page for troubleshooting |
| **Analytic** | Errors | Centralized display of verification errors, supporting one-click navigation to the problem site |

---

## 2. Installation and Startup

### 2.1 Prerequisites

HVRM Insight depends on data files output by the Checker plugin. Before use, ensure Checker has been fully executed and the following two data output switches are enabled in the Checker configuration file:

The Checker configuration file is located at `/path/to/hccl_vm_install/plugin/checker/manifest.json` by default.

```json
"setting": {
    "enable_insight_dump": true,
    "enable_memory_snapshot_dump": true
}
```

- **`enable_insight_dump`**: When enabled, Checker outputs the graph structure and verification result files required by Insight during execution.
- **`enable_memory_snapshot_dump`**: When enabled, Checker outputs memory snapshot files for each rank during execution.

Both switches default to `false`. If not enabled, Insight will not be able to read the complete analysis data.

### 2.2 Data Directory Structure

The tool reads datasets from the `data/` directory. Each dataset is an independent subdirectory:

```text
data/
└── <dataset_name>/
    ├── manifest.json        # Dataset metadata (required)
    ├── graph/               # Graph structure files (DAG)
    ├── memory/              # Memory snapshot files
    └── validation/
        └── issues.msgpack   # Verification error records (optional)
```

When optional directories are missing, the corresponding page displays an empty state.

### 2.3 Method 1: Via hccl-vm Plugin (Recommended)

```bash
# Install and start the backend service
hccl-vm plugin install insight
# After successful installation, the terminal outputs the access URL (default `http://localhost:8000`). Open it in your browser.

# View the access URL again
hccl-vm plugin run @insight

# Uninstall the plugin (also stops the backend service)
hccl-vm plugin uninstall @insight
```

### 2.4 Method 2: Local Direct Startup

```bash
# Enter the insight plugin directory
python3 serve.py
```

The backend listens on `http://localhost:8000` by default, serving both the frontend page and the data API.

### 2.5 Verify Successful Startup

After opening the URL in the browser, if you can see the dataset list on the Dashboard page, startup was successful. If the page is blank, check whether the `data/` directory exists and contains the `manifest.json` file.

---

## 3. Scenario Demonstrations

### 3.1 Scenario 1: Select Dataset and View Overview (Dashboard)

Browse all datasets, click to select a target dataset, view operator information and statistics, then enter the correlation analysis page.

![Dashboard Operation Demo](insight_image/dashboard.gif)

### 3.2 Scenario 2: Linked Analysis of Memory and Task Graph (MemView)

Through the linked display of the memory timeline and DAG task graph, locate memory operations for a specific step, view node details, and trace data sources.

![MemView Operation Demo](insight_image/memView.gif)

### 3.3 Scenario 3: View Errors and Navigate to the Problem Site (Analytic)

View error details on the Analytic page or the error list on the left side of MemView, then one-click navigate to the associated DAG node or memory context.

![Error Location Operation Demo](insight_image/error.gif)

---

## 4. Page Details

### 4.1 Dashboard Page

Dashboard is the starting point for using the tool. The page is divided into three areas from left to right:

- **Left**: Operator summary and Rank tree. The Rank tree supports selection/deselection, and the selection is synchronized to all pages.
- **Center**: Dataset list. Click a row to select a dataset; the left and right panels refresh simultaneously.
- **Right**: Details panel for the selected dataset, showing statistics and navigation buttons.

![Dashboard Overview](insight_image/dashboard_overview.png)

After selecting a dataset, detailed statistics are displayed on the right:

![Dashboard Selected Dataset](insight_image/dashboard_selected.png)

The bottom of the details panel provides two navigation entry points:

- **`Enter Correlation View`**: Navigate to MemView with the current dataset and rank selection.
- **`View Diagnostics`**: Navigate to Analytic to view all verification errors for this dataset.

### 4.2 MemView Page

MemView displays the memory timeline and DAG task graph in the same view for linked analysis. It is the core page for troubleshooting.

![MemView Overview](insight_image/memview_overview.png)

**Page Layout:**

- **Left**: Rank selection tree, issue list, search panel.
- **Top**: Memory timeline — shows the buffer state evolution for each rank by step.
- **Bottom**: DAG task graph — displays task nodes and dependencies in Rank/Stream swimlanes.
- **Right**: Details panel — shows detailed information for the selected step or node.

![DAG Node View](insight_image/memview_dag_node.png)

#### 4.3 Core Coordination Mechanism: Step, Rank, and Task Interplay

- Select a step on the timeline → DAG highlights the corresponding node synchronously.
- Click a node in the DAG → the right panel switches to node details.
- Click a navigation button on the right → the timeline locates the corresponding step synchronously.

**The right details panel has two modes**, automatically switching based on whether a DAG node is selected:

- **Step Detail Mode**: Displays the memory operation list (Task Memory Ops) and Buffer slice details (Buffer Layout) for the current step. Each slice in Buffer Layout can be expanded to view data source semantics, with click navigation support.
- **Node Detail Mode**: Displays node attributes, parent/child node relationships, cross-stage node mapping, and memory operations associated with the node.

**Other Features:**

- **Bottom Player**: Supports browsing by step order, with a draggable slider for quick jumping. Playback speed is 1 second/step.
- **Focus View**: When a step is selected, the DAG automatically enters a partial view showing only nodes related to the current step and their context. Suitable for narrowing down when nodes are dense.
- **Subgraph Browsing**: When a node contains a subgraph, click `Open Subgraph` on the right to view a finer-grained task structure.
- **Stage Switching**: Switch graph stages via the top dropdown (e.g., `input_graph` / `input_task_queues`).
- **Search**: Supports searching DAG nodes by taskId / taskType / notifyId. Clicking a result locates the node.

### 4.4 Analytic Page

Analytic centrally displays all errors found during verification. The page is divided into three areas:

![Analytic Error Details](insight_image/analytic_issue_detail.png)

- **Left**: Dataset and rank selection.
- **Center**: Error list, each showing title, severity label (color-coded), key field summary, and original error code.
- **Right**: Details panel for the selected error, including basic error information, involved ranks, associated node information, slice details (precise location of data inconsistencies), supplementary information, and raw JSON.

The top of the details panel provides navigation buttons:

- **`View memView`**: Navigate to MemView, locating the memory step associated with the error.
- **`View dagView`**: Navigate to MemView, directly locating the DAG node associated with the error.

Navigation automatically carries the dataset, rank, and node location information, so no re-selection is needed after arriving at MemView.

---

## 5. Common Usage Flows

### 5.1 Starting Analysis from Dashboard

1. Click the target dataset in the dataset list.
2. Select the ranks to view in the Rank tree on the left.
3. Confirm the dataset's file count and error statistics on the right.
4. Click `Enter Correlation View` to enter MemView.

### 5.2 Locating Memory Issues in MemView

1. Click the target step on the timeline (or use the bottom player to jump).
2. View the step's memory operations in the right `Task Memory Ops` section.
3. Select a rank and buffer type in `Buffer Layout`, expand a slice to view the data source.
4. Click `Locate Task` or the semantic card to jump to the corresponding DAG node.

### 5.3 Reverse Tracing Memory from a DAG Node

1. Click the target node in the DAG.
2. View the node's `Memory Ops` on the right.
3. Click `Jump to Step` to return to the timeline, or click parent/child nodes to continue tracing along the dependency chain.

### 5.4 Quick Location from Errors

1. Click an error in the Analytic page or the error list on the left of MemView.
2. View the associated node and slice information in the details.
3. Click `View memView` or `View dagView` to jump to the problem site.

---

## 6. Usage Tips

- First confirm the dataset and rank range in Dashboard, then enter MemView for analysis.
- When investigating memory issues, use `Task Memory Ops` and `Buffer Layout` together.
- When investigating node dependency relationships, first select the node in the DAG, then use the parent/child relationship and node mapping on the right to navigate.
- When DAG nodes are too dense, first use Focus View to narrow down, then expand gradually.
- The error list on the left of MemView allows quick browsing and error node location without leaving the current page.

---

## 7. Terminology

| Term | Description |
|------|-------------|
| **Rank** | A single compute unit (e.g., NPU). Each rank independently executes computation |
| **DAG** | Directed Acyclic Graph, describing dependency relationships between task nodes |
| **Step** | A time step on the timeline, corresponding to a memory state snapshot |
| **Buffer** | A data area in memory for storing computation or communication data |
| **Stage** | A phase of the graph, such as `input_graph` or `input_task_queues` |
| **Stream** | Task execution flow. Tasks within the same stream execute sequentially |
| **Issue** | An error or anomaly record found during verification |
