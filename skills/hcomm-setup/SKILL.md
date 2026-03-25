---
name: hcomm-setup-skill
description: 该 Skill 负责 HCOMM 仓库一键环境准备、依赖安装。
---

该 Skill 负责 HCOMM 仓库一键环境准备、依赖安装执行。

## Step 1. 检查依赖是否满足要求

逐个检查以下依赖项是否安装符合要求的版本，若未安装或版本不满足，请先安装/升级或降级。

- Ubuntu 20.04 
- Python 3.7-3.11
- pip >= 20.3
- setuptools >= 45
- wheel >= 0.34
- gcc == 7.3
- cmake >= 3.16
- pkg-config >= 0.29.1
- ccache（可选）

## Step2. 安装昇腾CANN包配套Toolkit套件

### 2.1下载toolkit包：
Ascend CANN Toolkit下载地址：https://ascend.devcloud.huaweicloud.com/artifactory/cann-run-mirror/software/master/
进入日期最新的目录
判断用户系统架构，进入对应目录下载toolkit包（aarch64或x86_64）

### 2.2 安装toolkit包：
询问用户是否指定安装路径，默认为否。
安装命令：bash Ascend-cann-toolkit_<version>_linux-<arch>.run --full --install-path=<install_path>
`<cann_version>`: 表示CANN包版本号。
`<arch>`: 表示CPU架构，如aarch64、x86_64。
`<install_path>`: 表示指定安装路径，可选，root用户默认安装在/usr/local/Ascend目录，指定路径安装时，指定的路径权限需设置为755。

### 2.3 设置CANN软件环境变量
询问用户是否以root用户安装，默认为是。

默认路径，root用户安装：source /usr/local/Ascend/cann/set_env.sh
默认路径，非root用户安装：source $HOME/Ascend/cann/set_env.sh

若用户指定了路径，则在指定路径下寻找set_env.sh并source.


