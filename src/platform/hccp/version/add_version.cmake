function(binary_add_version TARGET_NAME)
    get_target_property(TARGET_SO_NAME ${TARGET_NAME} LIBRARY_OUTPUT_NAME)
    if(NOT TARGET_SO_NAME)
        set(TARGET_SO_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}${TARGET_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()
    string(REPLACE "." "_" SAFE_TARGET_NAME "${TARGET_SO_NAME}")

    set(GEN_FILE "${CMAKE_CURRENT_BINARY_DIR}/version_${TARGET_NAME}.c")
    set(TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    set(COMPILER_INFO "${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")

    string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d%H%M%S" UTC)

    configure_file(
        "${CMAKE_SOURCE_DIR}/src/platform/hccp/version/version.c.in"
        "${GEN_FILE}"
        @ONLY
    )

    target_sources(${TARGET_NAME} PRIVATE "${GEN_FILE}")

endfunction()