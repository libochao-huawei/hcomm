set(stub_list
    stub_kms.c
)
# 创建共享库
add_library(stub_kms SHARED ${stub_list})

# 设置头文件搜索路径
target_include_directories(stub_kms PRIVATE
    libkmc/
)

install(TARGETS  stub_kms
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)