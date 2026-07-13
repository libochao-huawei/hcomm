# TABLE CMD

[toc]

## 1 Core Features

### 1.1 Starting the Simulation Environment

#### 1.1.1 Viewing All Table Names

```bash
(hvm)$> hccl-vm table show all
all
Server
Host
Runner
Device
...
(hvm)$>
```

#### 1.1.2 Viewing Specified Table Contents

```bash
(hvm)$> hccl-vm table show Device
| id | server_id | logic_id | physical_id | overflow_mode | soc_version | status |
| 1 | 1 | 0 | 0 | 0 | Ascend950 | 0 |
| 2 | 1 | 1 | 1 | 0 | Ascend950 | 0 |
| 3 | 1 | 2 | 2 | 0 | Ascend950 | 0 |
| 4 | 1 | 3 | 3 | 0 | Ascend950 | 0 |

(hvm)$>
```

#### 1.1.3 Updating Specified Content in a Specified Table Row (currently only supports modifying device soc_version)

```bash
(hvm)$> hccl-vm table show Device
| id | server_id | logic_id | physical_id | overflow_mode | soc_version | status |
| 1 | 1 | 0 | 0 | 0 | Ascend950 | 0 |
| 2 | 1 | 1 | 1 | 0 | Ascend950 | 0 |
| 3 | 1 | 2 | 2 | 0 | Ascend950 | 0 |
| 4 | 1 | 3 | 3 | 0 | Ascend950 | 0 |
(hvm)$> hccl-vm table update device 1 soc_version Ascend951
update device 1 soc_version Ascend951
Updating device [id=1].soc_version = "Ascend951"
(hvm)$> hccl-vm table show Device
| id | server_id | logic_id | physical_id | overflow_mode | soc_version | status |
| 1 | 1 | 0 | 0 | 0 | Ascend951 | 0 |
| 2 | 1 | 1 | 1 | 0 | Ascend950 | 0 |
| 3 | 1 | 2 | 2 | 0 | Ascend950 | 0 |
| 4 | 1 | 3 | 3 | 0 | Ascend950 | 0 |

(hvm)$>
```
