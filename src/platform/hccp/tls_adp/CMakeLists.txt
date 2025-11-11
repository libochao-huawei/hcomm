set(tls_adp_list
    rs_tls.c
    encrypt.c
    ssl_adp.c
)
add_library(tls_adp  SHARED ${tls_adp_list})

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/third_party/openssl.cmake)

target_include_directories(tls_adp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../inc/driver/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hccp/rdma_service/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../roce/roce_user
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../crypt/inc/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/private/network/

    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/network
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/private/network
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/
    ${CMAKE_CURRENT_SOURCE_DIR}/../crypt/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../roce/host_lite
    ${CMAKE_CURRENT_SOURCE_DIR}/../roce/roce_user
    ${CMAKE_BINARY_DIR}/openssl-install/include
    ${CMAKE_BINARY_DIR}/../third_party/openssl/include
    #暂时依赖文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/rdma-core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/abl/toolchain
    ${CMAKE_CURRENT_SOURCE_DIR}/../stub_kms/libkmc
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/driver/
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/inc/driver/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../externel_depends/driver/

    ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/slog/toolchain/
    ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/mmpa
    ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/ascend_hal/driver/
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/include
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/include/
    ${ASCEND_CANN_PACKAGE_PATH}/include/

    #缺runtime包临时引用放置在extenel_depends中的头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../externel_depends/c_sec/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/inc/log
)

add_dependencies(tls_adp crypto_static ssl_static)