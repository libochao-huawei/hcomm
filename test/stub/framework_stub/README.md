# HCCL框架层桩函数

## 概述

HCCL框架层桩函数主要是用于对框架层代码进行打桩，确保框架层的测试用例可以不依赖底层硬件并正常运行。

## 目录结构说明

```shell
framework_stub/
├── CMakeLists.txt
├── README.md
├── ranktable # ranktable配置项
├── workspace # 打桩所需.o文件以及json文件
├── *.cc # 桩函数源文件
└── *.h # 桩函数头文件
```