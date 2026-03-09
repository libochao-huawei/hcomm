cmake_minimum_required(VERSION 3.9)
project(topoaddrinfo VERSION 1.0.0 DESCRIPTION "topo addr info generator")

add_compile_options(-Wall -Wextra)

add_library(topoaddrinfo SHARED
    src/hal.c
    src/topo.c
    src/host_rdma.c
    src/pcie_nic.c
    src/eid_util.c
    src/rootinfo_types.c
    src/rankinfo.c
    src/product_card.c
    src/product_server.c
)

add_executable(test.out
    test/eid_test.cpp
    test/topo_test.cpp
    test/main.cpp
)

target_include_directories(test.out PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test.out PRIVATE rankinfo gtest pthread dl)
