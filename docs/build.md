# 源码构建

## 环境准备

1. 安装依赖。

   本项目编译用到的软件依赖如下，请注意版本要求。

   - python >= 3.7.0
   - gcc >= 7.3.0
   - cmake >= 3.16.0
   - ccache
   - nlohmann_json
   - googletest（仅执行UT时依赖，建议版本 release-1.14.0）

   nlohmann_json和googletest可通过`build_third_party.sh`进行编译安装，可通过`--output_path`参数指定安装目录，默认为`./output/third_party`：

   ```shell
   bash build_third_party.sh --output_path=${THIRD_LIB_PATH}
   ```

2. 安装CANN软件包。

   编译本项目依赖CANN开发套件包（cann-toolkit），支持的CANN软件版本为：`CANN 8.5.0.alpha002`，请从[资源下载中心](https://www.hiascend.com/developer/download/community/result?module=cann&cann=8.5.0.alpha002)下载`Ascend-cann-toolkit_${version}_linux-${arch}.run`软件包，并参考[昇腾文档中心-CANN软件安装指南](https://www.hiascend.com/document/redirect/CannCommunityInstWizard)进行安装。

3. 设置CANN软件环境变量。

   ```shell
   # root用户默认安装路径为：/usr/local/Ascend，普通用户默认按装路径为：$HOME/Ascend
   source /usr/local/Ascend/ascend-toolkit/set_env.sh
   ```

## 源码下载

```shell
# 下载项目源码，以master分支为例
git clone https://gitcode.com/cann/hcomm.git
```

## 编译

本项目提供一键式编译构建能力，进入代码仓根目录，执行如下命令：

```shell
bash build.sh --pkg
```

编译完成后会在`./build_out`目录下生成 `cann-hcomm_<version>_linux-<arch>.run` 软件包。

> `<version>`表示软件版本号，`<arch>`表示操作系统架构，取值包括“x86_64”与“aarch64”。

## 安装

安装编译生成的HCOMM软件包：

```shell
bash ./build_out/cann-hcomm_<version>_linux-<arch>.run
```

请注意：编译时需要将上述命令中的软件包名称替换为实际编译生成的软件包名称。

安装完成后，用户编译生成的HCOMM软件包会替换已安装CANN开发套件包中的HCOMM相关软件。

## LLT 测试

安装完编译生成的HCOMM软件包后，可通过如下命令执行LLT用例。

```shell
bash build.sh --ut
```

如需使能地址消毒器，可添加参数 `--asan`，命令如下：

```shell
bash build.sh --test --asan
```
