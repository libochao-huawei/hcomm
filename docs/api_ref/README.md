# HCOMM API总览

- [通信域创建与管理接口（C语言）](./comm_mgr_c/README.md)：用于实现单算子模式下的框架适配，实现分布式能力。
- [通信域创建与管理接口（Python语言）](./comm_mir_python/README.md)：用于实现图模式下的框架适配，当前仅用于TensorFlow网络在NPU执行分布式优化。
- [通信算子开发接口](./comm_opdev/README.md)：提供控制面接口与数据面接口，支撑开发者自定义通信算子。
