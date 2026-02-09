target_sources(hccl_v2 PRIVATE
        prim_queue.cc
        prim_translator.cc
        primitive.cc
        prim_rules.cc
)

if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
target_sources(ccl_kernel PRIVATE
        prim_queue.cc
        prim_translator.cc
        primitive.cc
        prim_rules.cc
)
endif()
