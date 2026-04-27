set(src_list
    dlurma_funciton.cc
    hccl_next_dl.cc
)

target_sources(hcomm PRIVATE
    ${src_list}
)