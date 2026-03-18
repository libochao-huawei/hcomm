cmake_minimum_required(VERSION 3.9)
project(topoaddrinfo VERSION 1.0.0 DESCRIPTION "topo addr info generator")

add_compile_options(-Wall -Wextra)

add_library(topoaddrinfo SHARED
    src/hal.c
    src/topo.c
    src/host_rdma.c
    src/pcie_nic.c
    src/eid_util.c
    src/rank_info_types.c
    src/topo_addr_info.c
    src/product_card.c
    src/product_server.c
)


target_include_directories(topoaddrinfo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${HCCL_BASE_DIR}/src/pub_inc
    ${HCCL_BASE_DIR}/include
    ${HCCL_BASE_DIR}/inc/adapter
    ${ASCEND_CANN_PACKAGE_PATH}/include/
    ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
)

target_link_libraries(topoaddrinfo PRIVATE
    dl
    c_sec
)

install(TARGETS topoaddrinfo
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
)
