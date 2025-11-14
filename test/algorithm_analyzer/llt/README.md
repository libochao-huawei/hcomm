如何编译LLT

1）删除历史目录，从头开始执行（命令执行路径为代码根目录）
rm -rf tmp && mkdir -p tmp && cd tmp && cmake ../cmake/superbuild/ -DCUSTOM_PYTHON=python3 -DHOST_PACKAGE=ut -DBUILD_MOD=hccl_checker -DFULL_COVERAGE=false -DCOVERAGE_RC_CONFIG=false && TARGETS=open_hccl_test  make -j20
rm -rf tmp && mkdir -p tmp && cd tmp && cmake ../cmake/superbuild/ -DCUSTOM_PYTHON=python3 -DHOST_PACKAGE=ut -DBUILD_MOD=hccl_checker -DFULL_COVERAGE=false -DCOVERAGE_RC_CONFIG=false && TARGETS=executor_hccl_test  make -j20
rm -rf tmp && mkdir -p tmp && cd tmp && cmake ../cmake/superbuild/ -DCUSTOM_PYTHON=python3 -DHOST_PACKAGE=ut -DBUILD_MOD=hccl_checker -DFULL_COVERAGE=false -DCOVERAGE_RC_CONFIG=false && TARGETS=executor_reduce_hccl_test  make -j20
rm -rf tmp && mkdir -p tmp && cd tmp && cmake ../cmake/superbuild/ -DCUSTOM_PYTHON=python3 -DHOST_PACKAGE=ut -DBUILD_MOD=hccl_checker -DFULL_COVERAGE=false -DCOVERAGE_RC_CONFIG=false && TARGETS=executor_pipeline_hccl_test  make -j20

2）不删除历史目录，再次执行（适用于测试代码的场景）(执行目录为tmp目录)
TARGETS=open_hccl_test  make -j20
TARGETS=executor_hccl_test  make -j20
TARGETS=executor_reduce_hccl_test  make -j20
TARGETS=executor_pipeline_hccl_test  make -j20

如何执行LLT
1）在tmp目录下执行
./llt_gccnative-prefix/src/llt_gccnative-build/ace/comop/hccl/open_source/test/open_hccl_test
./llt_gccnative-prefix/src/llt_gccnative-build/ace/comop/hccl/open_source/test/executor_hccl_test
./llt_gccnative-prefix/src/llt_gccnative-build/ace/comop/hccl/open_source/test/executor_reduce_hccl_test
./llt_gccnative-prefix/src/llt_gccnative-build/ace/comop/hccl/open_source/test/executor_pipeline_hccl_test

AIV相关测试命令，待完善相关功能
rm -rf tmp && mkdir -p tmp && cd tmp && cmake ../cmake/superbuild/ -DCUSTOM_PYTHON=python3 -DHOST_PACKAGE=ut -DBUILD_MOD=hccl_checker -DFULL_COVERAGE=false -DCOVERAGE_RC_CONFIG=false && TARGETS=hccl_alg_aiv_test  make -j20

TARGETS=hccl_alg_aiv_test  make -j20 VERBOSE=ON
