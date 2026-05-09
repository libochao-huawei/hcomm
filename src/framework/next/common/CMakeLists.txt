set(src_list
    dlurma_function.cc
    hccl_next_dl.cc
)

target_sources(hcomm PRIVATE
    ${src_list}
)