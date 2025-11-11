set(stub_list
    stub_kms.c
)
# 创建共享库
add_library(KMS SHARED ${stub_list})

# 设置头文件搜索路径
target_include_directories(KMS PRIVATE
    libkmc/
)

install(TARGETS  KMS
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)