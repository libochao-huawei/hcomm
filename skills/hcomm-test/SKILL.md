---
name: hcomm-test-skill
description: 该 Skill 负责 HCOMM 仓库源码编译、软件包安装/卸载、LLT 测试执行。
---

该 Skill 负责 HCOMM 仓库源码编译、软件包安装/卸载、LLT 测试执行。

## Step 1. 源码编译

进入代码仓根目录，执行编译命令。

### 1.1 基础编译

询问用户编译host包还是host+device包，默认为host包。

编译 host 包命令：
bash build.sh --pkg

编译 host + device 包命令：
bash build.sh --pkg --full

### 1.2 离线编译

若1.1基础编译成功则直接跳过此步
若编译环境无法访问网络，需告知用户要在联网环境中下载开源软件压缩包，手动上传至编译环境，并通过 `--cann_3rd_lib_path` 参数指定软件包路径
等待用户下载并上传压缩包至编译环境后，执行以下命令进行离线编译：

指定软件包路径，默认为：./third_party
bash build.sh --cann_3rd_lib_path={your_3rd_party_path}

### 1.3 编译结果检查
编译完成后会在 `./build_out` 目录下生成 `cann-hcomm_<version>_linux-<arch>.run` 软件包。

`<version>`: 表示软件版本号
`<arch>`: 表示操作系统架构，取值包括"x86_64"与"aarch64"

检查生成的软件包名称是否符合上述命名规范，若不符合，请检查编译日志排查原因。

## Step 2. 安装 HCOMM 软件包

安装编译生成的 HCOMM 软件包，命令：
bash ./build_out/cann-hcomm_<version>_linux-<arch>.run --full

请将命令中的软件包名称替换为实际编译生成的软件包名称。

安装完成后，用户编译生成的 HCOMM 软件包会替换已安装 CANN 开发套件包中的 HCOMM 相关软件。

## Step 3. 执行 LLT 测试

安装完编译生成的 HCOMM 软件包后，执行 LLT 用例：

执行UT测试命令：
bash build.sh --ut

执行ST测试命令：
bash build.sh --st

## Step 4. 卸载 HCOMM 软件包

询问用户是否卸载已安装的 HCOMM 软件包，卸载命令：
bash ./build_out/cann-hcomm_<version>_linux-<arch>.run --uninstall

请将命令中的软件包名称替换为实际安装的软件包名称。
