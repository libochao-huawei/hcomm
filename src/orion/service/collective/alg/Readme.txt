编译命令如下：
1）同时编译hccl库和算法库
mkdir tmp && cd tmp && cmake ../cmake/superbuild -DHOST_PACKAGE=fwkacllib && TARGETS=hccl_v2 make -j64 host

2）只编译算法库
mkdir tmp && cd tmp && cmake ../cmake/superbuild -DHOST_PACKAGE=fwkacllib && TARGETS=hccl_v2_alg_frame make -64 host


编译后二进制所在目录：
1）算法模板的so位置
./host-prefix/src/host-build/ace/comop/hccl/whole/hccl/v2/service/collective/alg/libhccl_v2_native_alg_repo.so

2）算法框架的so位置
./host-prefix/src/host-build/ace/comop/hccl/whole/hccl/v2/service/collective/alg/libhccl_v2_alg_frame.so
