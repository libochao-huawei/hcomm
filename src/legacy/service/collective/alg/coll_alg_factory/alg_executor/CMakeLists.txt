target_sources(hccl_v2_native_alg_repo PRIVATE

        executor_utils.cc

)
if((NOT BUILD_OPEN_PROJECT) OR DEVICE_MODE)
target_sources(ccl_kernel PRIVATE
        executor_utils.cc
)
endif()

add_subdirectory(prim_alg_executor)
add_subdirectory(ins_alg_executor)
