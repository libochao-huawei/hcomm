# device 和 host 侧都需要的文件
add_subdirectory(common)

# 仅 device 侧需要的文件
add_subdirectory(device)

# 仅 host 侧需要的文件
add_subdirectory(host)