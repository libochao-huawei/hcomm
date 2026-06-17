# 简介

本节包含CCU Kernel主机侧生命周期管理接口及内存Token查询接口。

通过这些接口，用户可以完成CCU实例的创建与销毁、Kernel的注册与翻译、Kernel的启动执行，以及进程虚拟地址到CCU访问Token的转换。

接口调用的标准顺序如下：

1. [HcommCcuKernelRegisterStart](HcommCcuKernelRegisterStart.md)：开始一轮Kernel注册。
2. [HcommCcuKernelRegister](HcommCcuKernelRegister.md)：注册Kernel函数，记录其操作序列。
3. [HcommCcuKernelRegisterEnd](HcommCcuKernelRegisterEnd.md)：结束本轮注册，翻译为设备指令并下发。
4. [HcommCcuKernelLaunch](HcommCcuKernelLaunch.md)：启动Kernel执行（可重复调用）。

旁路接口：

- [HcommCcuGetMemToken](HcommCcuGetMemToken.md)：将进程虚拟地址转换为CCU可用的内存Token，与主流程无依赖关系，可在任意时机调用。
