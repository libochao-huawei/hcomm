set(HCCL_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole)
set(HCCL_OPEN_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/open_source)
set(HCCL_LLT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(HCCL_NEXT_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole/hccl_next)

set(include_list
    # inc目录下头文件
    ${TOP_DIR}/inc
    ${TOP_DIR}/ace/comop/inc
    ${TOP_DIR}/ace/comop/inc/hccl
    ${TOP_DIR}/graphengine/inc
    ${TOP_DIR}/metadef/
    ${TOP_DIR}/metadef/inc
    ${TOP_DIR}/metadef/pkg_inc
    ${TOP_DIR}/inc/common/util/error_manager
    ${TOP_DIR}/metadef/inc/common/util/error_manager
    ${TOP_DIR}/metadef/inc/common/util/trace_manager
    ${TOP_DIR}/ace/npuruntime/runtime/platform/inc
    ${TOP_DIR}/inc/register
    ${TOP_DIR}/metadef/inc/register
    ${TOP_DIR}/inc/graph
    ${TOP_DIR}/metadef/inc/graph
    ${TOP_DIR}/inc/common/opskernel
    ${TOP_DIR}/metadef/inc/common/opskernel
    ${TOP_DIR}/inc/framework
    ${TOP_DIR}/graphengine/inc/framework
    ${TOP_DIR}/inc/external
    ${TOP_DIR}/metadef/inc/external
    ${TOP_DIR}/metadef/pkg_inc
    ${TOP_DIR}/graphengine/inc/external
    ${TOP_DIR}/inc/external/graph
    ${TOP_DIR}/metadef/inc/external/graph
    ${TOP_DIR}/ace/comop/hccl/whole/hccl/ai_cpu_kernel/framework/
    ${TOP_DIR}/inc/common
    ${TOP_DIR}/metadef/inc/common
    ${TOP_DIR}/metadef/inc/common/util
    ${TOP_DIR}/metadef/inc/exe_graph/lowering
    ${TOP_DIR}/inc/aicpu/
    ${TOP_DIR}/inc/aicpu/aicpu_schedule/aicpu_sharder
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/asl/hss/inc
    ${TOP_DIR}/asl/hss/inc/pub
    ${TOP_DIR}/asl/hss/common/hash_table
    ${TOP_DIR}/abl/qos/qosmng
    ${TOP_DIR}/air/inc/external
    ${TOP_DIR}/metadef/
    ${TOP_DIR}/metadef/inc/external/register
    # libc_sec目录下的头文件
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/abl/atrace/inc/utrace
    ${TOP_DIR}/inc/aicpu/common
    ${TOP_DIR}/ace/csruntime/inc/tsch
    ${TOP_DIR}/asc/asc-devkit
    ${TOP_DIR}/asc/asc-devkit/impl
    ${TOP_DIR}/asc/asc-devkit/include

    ${HCCL_LLT_ROOT}/stub
    ${TOP_DIR}/hlt/hccl/third_party/mellanox/4.2/include
    ${TOP_DIR}/open_source/json/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/mpi/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include/3rdparty
    ${TOP_DIR}/llt/ace/comop/hccl/stub/

    # # hccl/hccl目录下头文件
    ${HCCL_CODE_ROOT}/
    ${HCCL_CODE_ROOT}/hccl
    ${HCCL_CODE_ROOT}/hccl/inc
    ${HCCL_CODE_ROOT}/hccl/framework/hcom
    ${HCCL_CODE_ROOT}/hccl/hccl/op_base/src
    ${HCCL_CODE_ROOT}/hccl/common/src
    ${HCCL_CODE_ROOT}/hccl/common/src/host
    ${HCCL_CODE_ROOT}/hccl/common/src/topo
    ${HCCL_CODE_ROOT}/hccl/hccd
    ${HCCL_CODE_ROOT}/hccl/hccl/hccl_comm
    ${HCCL_CODE_ROOT}/hccl/algorithm/pub_inc
    ${HCCL_CODE_ROOT}/hccl/algorithm/impl

    ${HCCL_CODE_ROOT}/hccl/hccl/hccl_comm/inc
    ${HCCL_CODE_ROOT}/hccl/hccl/hccl_comm/executor/component/inc
    ${HCCL_CODE_ROOT}/hccl/hccl/hccl_comm/impl
    ${HCCL_CODE_ROOT}/hccl/hdcs
    ${HCCL_CODE_ROOT}/hccl/platform/
    ${HCCL_CODE_ROOT}/hccl/platform/common/
    ${HCCL_CODE_ROOT}/hccl/platform/common/unique/
    ${HCCL_CODE_ROOT}/hccl/platform/common/buffer_manager
    ${HCCL_CODE_ROOT}/hccl/platform/common/unfold_cache
    ${HCCL_CODE_ROOT}/hccl/platform/ping_mesh/
    ${HCCL_CODE_ROOT}/hccl/platform/inc/
    ${HCCL_CODE_ROOT}/hccl/platform/inc/adapter
    ${HCCL_CODE_ROOT}/hccl/platform/inc/adapter/host
    ${HCCL_CODE_ROOT}/hccl/platform/resource/
    ${HCCL_CODE_ROOT}/hccl/platform/resource/mem
    ${HCCL_CODE_ROOT}/hccl/platform/resource/notify/
    ${HCCL_CODE_ROOT}/hccl/platform/resource/rma_buffer
    ${HCCL_CODE_ROOT}/hccl/platform/resource/transport/
    ${HCCL_CODE_ROOT}/hccl/platform/resource/transport/device/
    ${HCCL_CODE_ROOT}/hccl/platform/resource/transport/host/
    ${HCCL_CODE_ROOT}/hccl/platform/resource/transport/heterog/
    ${HCCL_CODE_ROOT}/hccl/platform/resource/transport/onesided
    ${HCCL_CODE_ROOT}/hccl/platform/resource/transport/onesided/device
    ${HCCL_CODE_ROOT}/hccl/platform/resource/socket/
    ${HCCL_CODE_ROOT}/hccl/platform/task/
    ${HCCL_CODE_ROOT}/hccl/platform/aiv_communication/
    ${HCCL_CODE_ROOT}/hccl/platform/debug/
    ${HCCL_CODE_ROOT}/hccl/platform/debug/dfx/
    ${HCCL_CODE_ROOT}/hccl/platform/debug/dfx/trace/
    ${HCCL_CODE_ROOT}/hccl/platform/task/
    ${HCCL_CODE_ROOT}/hccl/platform/task/rtsq_interact/
    ${HCCL_CODE_ROOT}/hccl/platform/typical

    ${HCCL_CODE_ROOT}/hccl/pub_inc
    ${HCCL_CODE_ROOT}/hccl/pub_inc/inner
    ${HCCL_CODE_ROOT}/hccl/pub_inc/aicpu
    ${HCCL_CODE_ROOT}/hccl/pub_inc/new
    ${HCCL_CODE_ROOT}/hccl/platform/resource/dispatcher_ctx
    ${HCCL_CODE_ROOT}/hccl/hccl_heterog/rpc/
    ${HCCL_CODE_ROOT}/hccl/hccl_heterog/heterog_cpu/
    ${HCCL_CODE_ROOT}/hccl/hccl_heterog/common/

    ${HCCL_OPEN_CODE_ROOT}/inc/hccl
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/inc/
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/inc/host
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/health/heartbeat/
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/host
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/task
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/exception
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/config
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/onesided_memory_management
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/one_sided_service
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/resource_manager
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/hcom/
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/op_base/src/
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/nslbdp/
    ${HCCL_OPEN_CODE_ROOT}/pkg_inc
    
    # hccl/common 目录下头文件
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/common/health
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/inc
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/inc/host
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/config
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/common/stream/
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/common/launch_device/

    # hccl/algorithm 目录下头文件
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/inc
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_reduce
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_alltoall
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_alltoallv
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_broadcast
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_gather
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_scatter
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_send_recv
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/component
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/component/inc
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/inc_all_reduce_deter
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/mc2_handler
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/inc
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/legacy
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/legacy/operator
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/task
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/registry
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/registry
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_send_receive
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/310P
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/310P
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor//coll_reduce_scatter/310P
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/310P

    # hccl/hcom_graph_adaptor目录下头文件
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor/hcom_executor
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor/ge_plugin
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/common
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hvd
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/auto_tuning
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor/evaluator
    ${HCCL_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/lowering

    # hccl/hcom_gradient_split_tune目录下头文件
    ${HCCL_CODE_ROOT}/hcom_gradient_split_tune/inc

    # hccl/cltm目录下头文件
    ${HCCL_CODE_ROOT}/cltm/src

    # hccl/operator目录下头文件
    ${HCCL_CODE_ROOT}/operator

    # proto.h头文件
    ${CMAKE_BINARY_DIR}/proto/hccl_llt

    # /hccl/framework/device/目录下头文件
    ${HCCL_CODE_ROOT}/hccl/framework/device/
    ${HCCL_CODE_ROOT}/hccl/framework/device/inc
    ${HCCL_CODE_ROOT}/hccl/framework/device/aicpu_kfc
    ${HCCL_CODE_ROOT}/hccl/framework/device/aicpu_kfc/inc
    ${HCCL_CODE_ROOT}/hccl/framework/device/aicpu_kfc/common
    ${HCCL_CODE_ROOT}/hccl/framework/device/common
    ${HCCL_CODE_ROOT}/hccl/framework/device/utils
    ${HCCL_CODE_ROOT}/hccl/framework/device/framework
    # dump工具头文件
    ${TOP_DIR}/abl/adump/inc/adump

    # orion include
    ${HCCL_CODE_ROOT}/orion/common/utils
    ${HCCL_CODE_ROOT}/orion/common/types

    # hccl_next include
    ${HCCL_NEXT_CODE_ROOT}/include
    # tbe头文件
    ${HCCL_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/
    ${HCCL_CODE_ROOT}/hccl/platform/legacy/common/
    ${HCCL_CODE_ROOT}/hccl/platform/legacy/graph_ctx_mgr/
    ${HCCL_CODE_ROOT}/hccl/platform/legacy/inc/
)

add_subdirectory(prepare_st_env)
add_subdirectory(single_test/misc)
add_subdirectory(mpi_test/misc)
add_subdirectory(mpi_test/comm)
add_subdirectory(platform_test/aicpu_test)
add_subdirectory(mc2_test/misc)
add_subdirectory(mc2_test/aicpu)
add_subdirectory(single_test/algorithms/910B)
add_subdirectory(single_test/algorithms/8pring)
add_subdirectory(single_test/comm)
add_subdirectory(single_test/coll)
add_subdirectory(single_test/impl)
add_subdirectory(single_test/executor/misc)
add_subdirectory(single_test/executor/inter)
add_subdirectory(single_test/hcom)
add_subdirectory(single_test/ping_mesh)
add_subdirectory(binary_package)
add_subdirectory(device)