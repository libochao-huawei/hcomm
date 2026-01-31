# 此处不包含alg文件夹， 算法平台单独打包为独立so文件
# 基础框架只引入部分头文件

add_subdirectory(instruction)
add_subdirectory(primitive)
add_subdirectory(alg)

target_sources(hccl_v2
        PRIVATE
        coll_operator.cc
        coll_operator_check.cc
)

if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
        coll_operator.cc
        coll_operator_check.cc
)
endif()