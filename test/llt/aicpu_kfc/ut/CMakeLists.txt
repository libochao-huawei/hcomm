set(AICPU_KFC_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole)
set(AICPU_KFC_OPEN_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/open_source)
set(AICPU_KFC_LLT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(AICPU_KFC_ORION_ROOT ${TOP_DIR}/ace/comop/hccl/orion)
set(AICPU_KFC_ALG_V2_CODE_ROOT ${AICPU_KFC_ORION_ROOT}/service/collective)

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
    ${TOP_DIR}/graphengine/inc/external
    ${TOP_DIR}/inc/external/graph
    ${TOP_DIR}/metadef/inc/external/graph
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
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl
    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/include

    ${AICPU_KFC_LLT_ROOT}/stub
    ${TOP_DIR}/hlt/hccl/third_party/mellanox/4.2/include
    ${TOP_DIR}/open_source/json/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/googletest/include
    ${TOP_DIR}/llt/third_party/mpi/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include
    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include/3rdparty
    ${TOP_DIR}/llt/ace/comop/hccl/aicpu_kfc/stub/

    # # hccl/hccl目录下头文件
    ${AICPU_KFC_CODE_ROOT}/
    ${AICPU_KFC_CODE_ROOT}/hccl
    ${AICPU_KFC_CODE_ROOT}/hccl/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/hcom/src
    ${AICPU_KFC_CODE_ROOT}/hccl/hcom/src/gradient_segment
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/op_base/src
    ${AICPU_KFC_CODE_ROOT}/hccl/hccd
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/pub_inc
    ${AICPU_KFC_CODE_ROOT}/hccl/algorithm/impl

    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/executor/component/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl/hccl_comm/impl
    ${AICPU_KFC_CODE_ROOT}/hccl/hdcs
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/ping_mesh/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/unique/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/common/buffer_manager
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc/adapter
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/inc/adapter/host
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/mem
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/host/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/device/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/heterog/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/transport/onesided/device
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/notify/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/socket/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/rma_buffer
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/resource/dispatcher_ctx
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/aiv_communication/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/debug/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/debug/dfx/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/debug/dfx/trace/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/task/rtsq_interact/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/typical
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/remote_access/
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl_heterog/rpc/
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl_heterog/heterog_cpu/
    ${AICPU_KFC_CODE_ROOT}/hccl/hccl_heterog/common/

    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/inc/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/inc/host
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/health/heartbeat/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/recovery/operator_retry
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/cluster_maintenance/snapshot
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/host
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/task
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/topo
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/exception
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/config
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/common/src/onesided_memory_management
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/one_sided_service
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/communicator/impl/resource_manager
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/hcom/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/op_base/src/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/framework/nslbdp/
    ${AICPU_KFC_OPEN_CODE_ROOT}/inc/hccl
    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc
    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc/inner
    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc/aicpu
    ${AICPU_KFC_CODE_ROOT}/hccl/pub_inc/new

    ${TOP_DIR}/inc/aicpu/common
    ${TOP_DIR}/ace/csruntime/inc/tsch
    # hccl/algorithm 目录下头文件
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_gather
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_all_reduce
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_alltoall
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_alltoallv
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_broadcast
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_gather
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_reduce_scatter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_scatter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/temp_send_recv
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/component
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/component/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_template/inc_all_reduce_deter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/mc2_handler
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/legacy
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/legacy/operator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/communicator/legacy
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/task
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/operator/registry
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/resource_manager
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/registry
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_gather_v/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_send_receive
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_scatter
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/impl/coll_executor/coll_broadcast/310P

    # hccl/common目录下头文件
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/health
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/inc
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/profiling/inc/host
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/debug/config
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/stream/
    ${AICPU_KFC_OPEN_CODE_ROOT}/src/domain/collective_communication/common/launch_device/

    # hccl/hcom_graph_adaptor目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/hcom_executor
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/common
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hvd
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/auto_tuning
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/calculation_tool
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/evaluator
    ${AICPU_KFC_CODE_ROOT}/hcom_graph_adaptor/ge_plugin/hcom/lowering

    # hccl/hcom_gradient_split_tune目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hcom_gradient_split_tune/inc

    # hccl/cltm目录下头文件
    ${AICPU_KFC_CODE_ROOT}/cltm/src

    # hccl/operator目录下头文件
    ${AICPU_KFC_CODE_ROOT}/operator

    # proto.h头文件
    ${CMAKE_BINARY_DIR}/proto/aicpu_kfc_llt
    ${CMAKE_BINARY_DIR}/proto/aicpu_kfc_llt_open

    # hccl/ai_cpu_kernel目录下头文件
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/aicpu_kfc
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/aicpu_kfc/inc
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/aicpu_kfc/common
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/common
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/utils
    ${AICPU_KFC_CODE_ROOT}/hccl/framework/device/framework

    # dump头文件
    ${TOP_DIR}/abl/adump/inc/adump

    # orion include
    ${AICPU_KFC_CODE_ROOT}/orion/common/utils
    ${AICPU_KFC_CODE_ROOT}/orion/common/types

    # AICPU_KFC_next include
    ${AICPU_KFC_NEXT_CODE_ROOT}/include

    # tbe头文件
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/hccl_tbe_task/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/common/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/graph_ctx_mgr/
    ${AICPU_KFC_CODE_ROOT}/hccl/platform/legacy/inc/
)

add_subdirectory(prepare_ut_env)
add_subdirectory(aicpu)
add_subdirectory(misc)
