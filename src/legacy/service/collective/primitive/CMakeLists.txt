target_sources(hccl_v2 PRIVATE
        prim_queue.cc
        prim_translator.cc
        primitive.cc
        prim_rules.cc
)

if(TARGET ccl_kernel)
target_sources(ccl_kernel PRIVATE
        prim_queue.cc
        prim_translator.cc
        primitive.cc
        prim_rules.cc
)
endif()
